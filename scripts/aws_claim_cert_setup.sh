#!/bin/bash
###############################################################################
# aws_claim_cert_setup.sh
#
# Complete setup for AWS IoT Fleet Provisioning (Provisioning by Claim).
# Creates:
#   Phase 1 - AWS Infrastructure:
#     1. IAM role for fleet provisioning
#     2. Device IoT policy (attached to provisioned things)
#     3. Fleet provisioning template
#   Phase 2 - Claim Certificate:
#     4. Claim certificate + private key
#     5. Claim IoT policy (attached to claim certificate)
#
# Prerequisites:
#   - OpenSSL installed
#   - AWS CLI v2 installed and configured (aws configure)
#   - Appropriate IAM permissions for IoT and IAM operations
#
# Usage:
#   chmod +x aws_claim_cert_setup.sh
#   ./aws_claim_cert_setup.sh [--region <region>] [--template <name>]
#                              [--policy <name>] [--out-dir <dir>]
###############################################################################
set -euo pipefail

#------------------------------------------------------------------------------
# Default Configuration
#------------------------------------------------------------------------------
AWS_REGION="${AWS_REGION:-us-east-1}"
PROVISIONING_TEMPLATE_NAME="FleetProvisioningTemplate"
PROVISIONING_ROLE_NAME="FleetProvisioningRole"
CLAIM_POLICY_NAME="ClaimCertificatePolicy"
DEVICE_POLICY_NAME="DevicePolicy"
OUTPUT_DIR="./aws_claim_certs"
KEY_CURVE="prime256v1"  # ECC P-256 (secp256r1)

#------------------------------------------------------------------------------
# Parse command-line arguments
#------------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --region)       AWS_REGION="$2"; shift 2 ;;
        --template)     PROVISIONING_TEMPLATE_NAME="$2"; shift 2 ;;
        --policy)       CLAIM_POLICY_NAME="$2"; shift 2 ;;
        --out-dir)      OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [--region region] [--template name] [--policy name] [--out-dir dir]"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

#------------------------------------------------------------------------------
# Derived paths
#------------------------------------------------------------------------------
CLAIM_KEY="${OUTPUT_DIR}/claim_private.pem.key"
CLAIM_CSR="${OUTPUT_DIR}/claim.csr"
CLAIM_CERT="${OUTPUT_DIR}/claim_certificate.pem.crt"
ROOT_CA="${OUTPUT_DIR}/AmazonRootCA1.pem"
CLAIM_CERT_ARN_FILE="${OUTPUT_DIR}/claim_cert_arn.txt"
PROVISIONING_TEMPLATE_FILE="${OUTPUT_DIR}/provisioning_template.json"
CLAIM_POLICY_FILE="${OUTPUT_DIR}/claim_policy.json"
DEVICE_POLICY_FILE="${OUTPUT_DIR}/device_policy.json"
TRUST_POLICY_FILE="${OUTPUT_DIR}/trust_policy.json"
ROLE_POLICY_FILE="${OUTPUT_DIR}/role_policy.json"
C_HEADER="${OUTPUT_DIR}/aws_claim_credentials.h"

echo "============================================================"
echo " AWS IoT Fleet Provisioning - Complete Setup"
echo "============================================================"
echo " Region   : ${AWS_REGION}"
echo " Template : ${PROVISIONING_TEMPLATE_NAME}"
echo " IAM Role : ${PROVISIONING_ROLE_NAME}"
echo " Claim Pol: ${CLAIM_POLICY_NAME}"
echo " Dev Pol  : ${DEVICE_POLICY_NAME}"
echo " Key Type : ECC P-256"
echo " Output   : ${OUTPUT_DIR}"
echo "============================================================"

mkdir -p "${OUTPUT_DIR}"

#------------------------------------------------------------------------------
# Helper: Delete non-default policy versions to stay under the 5-version limit
#------------------------------------------------------------------------------
cleanup_policy_versions() {
    local policy_name="$1"
    local versions
    versions=$(aws iot list-policy-versions \
        --policy-name "${policy_name}" \
        --region "${AWS_REGION}" \
        --query 'policyVersions[?isDefaultVersion==`false`].versionId' \
        --output text 2>/dev/null) || return 0
    for ver in ${versions}; do
        echo "    Deleting old policy version ${ver} of '${policy_name}'..."
        aws iot delete-policy-version \
            --policy-name "${policy_name}" \
            --policy-version-id "${ver}" \
            --region "${AWS_REGION}" 2>/dev/null || true
    done
}

#------------------------------------------------------------------------------
# Helper: Deactivate and detach old claim certificate from previous run
#------------------------------------------------------------------------------
cleanup_old_claim_cert() {
    if [[ -f "${CLAIM_CERT_ARN_FILE}" ]]; then
        local old_arn
        old_arn=$(cat "${CLAIM_CERT_ARN_FILE}" 2>/dev/null)
        if [[ -n "${old_arn}" ]]; then
            local old_id
            old_id=$(echo "${old_arn}" | sed 's|.*/||')
            echo "  Cleaning up previous claim certificate: ${old_id}"
            # Detach all policies
            local policies
            policies=$(aws iot list-attached-policies \
                --target "${old_arn}" \
                --region "${AWS_REGION}" \
                --query 'policies[].policyName' \
                --output text 2>/dev/null) || true
            for pol in ${policies}; do
                echo "    Detaching policy '${pol}' from old certificate..."
                aws iot detach-policy \
                    --policy-name "${pol}" \
                    --target "${old_arn}" \
                    --region "${AWS_REGION}" 2>/dev/null || true
            done
            # Deactivate the certificate
            aws iot update-certificate \
                --certificate-id "${old_id}" \
                --new-status INACTIVE \
                --region "${AWS_REGION}" 2>/dev/null || true
            # Delete the certificate
            aws iot delete-certificate \
                --certificate-id "${old_id}" \
                --region "${AWS_REGION}" 2>/dev/null || true
            echo "  Old certificate cleaned up."
        fi
    fi
}

###############################################################################
# PHASE 1: AWS Infrastructure (IAM Role, Policies, Template)
###############################################################################

#------------------------------------------------------------------------------
# Step 1: Get AWS IoT endpoint and account ID
#------------------------------------------------------------------------------
echo ""
echo "[Step 1/12] Retrieving AWS IoT endpoint and account info..."
IOT_ENDPOINT=$(aws iot describe-endpoint \
    --endpoint-type iot:Data-ATS \
    --region "${AWS_REGION}" \
    --query 'endpointAddress' \
    --output text)
AWS_ACCOUNT_ID=$(aws sts get-caller-identity --query 'Account' --output text)
echo "  Endpoint  : ${IOT_ENDPOINT}"
echo "  Account ID: ${AWS_ACCOUNT_ID}"

#------------------------------------------------------------------------------
# Step 2: Create IAM Role for Fleet Provisioning
#------------------------------------------------------------------------------
echo ""
echo "[Step 2/12] Creating IAM role for Fleet Provisioning..."

# Trust policy: allows IoT service to assume this role
cat > "${TRUST_POLICY_FILE}" << TRUST_EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Principal": {
                "Service": "iot.amazonaws.com"
            },
            "Action": "sts:AssumeRole"
        }
    ]
}
TRUST_EOF

# Role permissions policy
cat > "${ROLE_POLICY_FILE}" << ROLEPOLICY_EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "iot:CreateThing",
                "iot:DescribeThing",
                "iot:AddThingToThingGroup",
                "iot:AttachThingPrincipal",
                "iot:AttachPolicy",
                "iot:DescribeCertificate",
                "iot:UpdateCertificate",
                "iot:CreatePolicy",
                "iot:GetPolicy"
            ],
            "Resource": "*"
        }
    ]
}
ROLEPOLICY_EOF

if aws iam get-role --role-name "${PROVISIONING_ROLE_NAME}" >/dev/null 2>&1; then
    echo "  IAM role '${PROVISIONING_ROLE_NAME}' already exists."
    PROVISIONING_ROLE_ARN=$(aws iam get-role \
        --role-name "${PROVISIONING_ROLE_NAME}" \
        --query 'Role.Arn' --output text)
else
    echo "  Creating IAM role '${PROVISIONING_ROLE_NAME}'..."
    PROVISIONING_ROLE_ARN=$(aws iam create-role \
        --role-name "${PROVISIONING_ROLE_NAME}" \
        --assume-role-policy-document "file://${TRUST_POLICY_FILE}" \
        --description "IAM role for AWS IoT Fleet Provisioning" \
        --query 'Role.Arn' --output text)
    echo "  Role created: ${PROVISIONING_ROLE_ARN}"
    echo "  Waiting for IAM role to propagate..."
    sleep 10
fi

# Attach/update the inline policy on the role
echo "  Attaching inline policy to role..."
aws iam put-role-policy \
    --role-name "${PROVISIONING_ROLE_NAME}" \
    --policy-name "FleetProvisioningPolicy" \
    --policy-document "file://${ROLE_POLICY_FILE}"

# Attach the AWS managed policy for fleet provisioning. The inline policy above
# lacks actions RegisterThing needs (e.g. UpdateThing, ListThingPrincipals when
# re-provisioning an existing thing), causing "provisioning role doesn't have
# the correct permissions" errors.
echo "  Attaching AWSIoTThingsRegistration managed policy to role..."
aws iam attach-role-policy \
    --role-name "${PROVISIONING_ROLE_NAME}" \
    --policy-arn "arn:aws:iam::aws:policy/service-role/AWSIoTThingsRegistration"
echo "  IAM role ready: ${PROVISIONING_ROLE_ARN}"

#------------------------------------------------------------------------------
# Step 3: Create Device IoT Policy (attached to provisioned things)
#------------------------------------------------------------------------------
echo ""
echo "[Step 3/12] Creating device IoT policy '${DEVICE_POLICY_NAME}'..."

cat > "${DEVICE_POLICY_FILE}" << DEVPOLICY_EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": "iot:Connect",
            "Resource": "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:client/*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "iot:Publish",
                "iot:Receive"
            ],
            "Resource": "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:topic/*"
        },
        {
            "Effect": "Allow",
            "Action": "iot:Subscribe",
            "Resource": "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:topicfilter/*"
        }
    ]
}
DEVPOLICY_EOF

if aws iot get-policy --policy-name "${DEVICE_POLICY_NAME}" --region "${AWS_REGION}" >/dev/null 2>&1; then
    echo "  Device policy '${DEVICE_POLICY_NAME}' already exists."
    cleanup_policy_versions "${DEVICE_POLICY_NAME}"
    echo "  Creating new default version..."
    aws iot create-policy-version \
        --policy-name "${DEVICE_POLICY_NAME}" \
        --policy-document "file://${DEVICE_POLICY_FILE}" \
        --set-as-default \
        --region "${AWS_REGION}" >/dev/null 2>&1 || \
    echo "  Warning: Could not update device policy. Using existing."
else
    aws iot create-policy \
        --policy-name "${DEVICE_POLICY_NAME}" \
        --policy-document "file://${DEVICE_POLICY_FILE}" \
        --region "${AWS_REGION}" >/dev/null
    echo "  Device policy '${DEVICE_POLICY_NAME}' created."
fi

#------------------------------------------------------------------------------
# Step 4: Create Fleet Provisioning Template
#------------------------------------------------------------------------------
echo ""
echo "[Step 4/12] Creating Fleet Provisioning template..."

cat > "${PROVISIONING_TEMPLATE_FILE}" << TEMPLATE_EOF
{
    "Parameters": {
        "ThingName": {
            "Type": "String"
        },
        "SerialNumber": {
            "Type": "String",
            "Default": "unknown"
        },
        "AWS::IoT::Certificate::Id": {
            "Type": "String"
        }
    },
    "Resources": {
        "thing": {
            "Type": "AWS::IoT::Thing",
            "Properties": {
                "ThingName": {
                    "Ref": "ThingName"
                },
                "AttributePayload": {
                    "SerialNumber": {
                        "Ref": "SerialNumber"
                    }
                }
            }
        },
        "certificate": {
            "Type": "AWS::IoT::Certificate",
            "Properties": {
                "CertificateId": {
                    "Ref": "AWS::IoT::Certificate::Id"
                },
                "Status": "Active"
            }
        },
        "policy": {
            "Type": "AWS::IoT::Policy",
            "Properties": {
                "PolicyName": "${DEVICE_POLICY_NAME}"
            }
        }
    }
}
TEMPLATE_EOF

if aws iot describe-provisioning-template \
    --template-name "${PROVISIONING_TEMPLATE_NAME}" \
    --region "${AWS_REGION}" >/dev/null 2>&1; then
    # Compare existing template body with the new one
    EXISTING_BODY=$(aws iot describe-provisioning-template \
        --template-name "${PROVISIONING_TEMPLATE_NAME}" \
        --region "${AWS_REGION}" \
        --query 'templateBody' --output text 2>/dev/null)
    NEW_BODY=$(cat "${PROVISIONING_TEMPLATE_FILE}")
    # Normalize JSON for comparison (remove whitespace)
    EXISTING_NORM=$(echo "${EXISTING_BODY}" | python -c "import sys,json; print(json.dumps(json.load(sys.stdin),sort_keys=True))" 2>/dev/null || echo "")
    NEW_NORM=$(echo "${NEW_BODY}" | python -c "import sys,json; print(json.dumps(json.load(sys.stdin),sort_keys=True))" 2>/dev/null || echo "")
    if [[ "${EXISTING_NORM}" == "${NEW_NORM}" ]]; then
        echo "  Provisioning template '${PROVISIONING_TEMPLATE_NAME}' already exists and is up-to-date. Skipping."
        PROVISIONING_TEMPLATE_SKIPPED=true
    else
        echo "  Provisioning template '${PROVISIONING_TEMPLATE_NAME}' exists but differs. Recreating..."
        aws iot delete-provisioning-template \
            --template-name "${PROVISIONING_TEMPLATE_NAME}" \
            --region "${AWS_REGION}"
        sleep 2
        PROVISIONING_TEMPLATE_SKIPPED=false
    fi
else
    PROVISIONING_TEMPLATE_SKIPPED=false
fi

if [[ "${PROVISIONING_TEMPLATE_SKIPPED}" != "true" ]]; then
    echo "  Creating provisioning template '${PROVISIONING_TEMPLATE_NAME}'..."
    aws iot create-provisioning-template \
        --template-name "${PROVISIONING_TEMPLATE_NAME}" \
        --provisioning-role-arn "${PROVISIONING_ROLE_ARN}" \
        --template-body "file://${PROVISIONING_TEMPLATE_FILE}" \
        --enabled \
        --region "${AWS_REGION}" >/dev/null
    echo "  Provisioning template created successfully."
fi

###############################################################################
# PHASE 2: Claim Certificate Creation
###############################################################################

#------------------------------------------------------------------------------
# Step 5: Clean up old claim certificate (from previous run)
#------------------------------------------------------------------------------
echo ""
echo "[Step 5/12] Cleaning up previous claim certificate (if any)..."
cleanup_old_claim_cert

#------------------------------------------------------------------------------
# Step 6: Generate ECC private key
#------------------------------------------------------------------------------
echo ""
echo "[Step 6/12] Generating ECC P-256 private key..."
openssl ecparam -genkey -name "${KEY_CURVE}" -noout -out "${CLAIM_KEY}"
echo "  EC P-256 key generated: ${CLAIM_KEY}"
chmod 600 "${CLAIM_KEY}"

#------------------------------------------------------------------------------
# Step 6: Generate CSR
#------------------------------------------------------------------------------
echo ""
echo "[Step 7/12] Generating Certificate Signing Request (CSR)..."
openssl req -new \
    -key "${CLAIM_KEY}" \
    -out "${CLAIM_CSR}" \
    -subj "/CN=ClaimCert/O=MyOrg/C=US"
echo "  CSR generated: ${CLAIM_CSR}"

#------------------------------------------------------------------------------
# Step 7: Create certificate from CSR via AWS IoT
#------------------------------------------------------------------------------
echo ""
echo "[Step 8/12] Creating certificate from CSR in AWS IoT..."
CREATE_RESULT=$(aws iot create-certificate-from-csr \
    --region "${AWS_REGION}" \
    --certificate-signing-request "file://${CLAIM_CSR}" \
    --set-as-active \
    --output json)

CERT_ARN=$(echo "${CREATE_RESULT}" | python -c "import sys,json; print(json.load(sys.stdin)['certificateArn'])")
CERT_ID=$(echo "${CREATE_RESULT}" | python -c "import sys,json; print(json.load(sys.stdin)['certificateId'])")
CERT_PEM=$(echo "${CREATE_RESULT}" | python -c "import sys,json; print(json.load(sys.stdin)['certificatePem'])")

echo "${CERT_PEM}" > "${CLAIM_CERT}"
echo "${CERT_ARN}" > "${CLAIM_CERT_ARN_FILE}"

echo "  Certificate ID  : ${CERT_ID}"
echo "  Certificate ARN : ${CERT_ARN}"
echo "  Certificate PEM : ${CLAIM_CERT}"

#------------------------------------------------------------------------------
# Step 8: Download Amazon Root CA
#------------------------------------------------------------------------------
echo ""
echo "[Step 9/12] Downloading Amazon Root CA..."
curl -sS -o "${ROOT_CA}" \
    "https://www.amazontrust.com/repository/AmazonRootCA1.pem"
echo "  Root CA saved: ${ROOT_CA}"

#------------------------------------------------------------------------------
# Step 9: Create Claim Certificate IoT Policy
#------------------------------------------------------------------------------
echo ""
echo "[Step 10/12] Creating IoT policy for claim certificate..."

cat > "${CLAIM_POLICY_FILE}" << POLICY_EOF
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": "iot:Connect",
            "Resource": "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:client/*"
        },
        {
            "Effect": "Allow",
            "Action": [
                "iot:Publish",
                "iot:Receive"
            ],
            "Resource": [
                "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:topic/\$aws/certificates/create-from-csr/*",
                "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:topic/\$aws/provisioning-templates/${PROVISIONING_TEMPLATE_NAME}/provision/*"
            ]
        },
        {
            "Effect": "Allow",
            "Action": "iot:Subscribe",
            "Resource": [
                "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:topicfilter/\$aws/certificates/create-from-csr/*",
                "arn:aws:iot:${AWS_REGION}:${AWS_ACCOUNT_ID}:topicfilter/\$aws/provisioning-templates/${PROVISIONING_TEMPLATE_NAME}/provision/*"
            ]
        }
    ]
}
POLICY_EOF

if aws iot get-policy --policy-name "${CLAIM_POLICY_NAME}" --region "${AWS_REGION}" >/dev/null 2>&1; then
    echo "  Policy '${CLAIM_POLICY_NAME}' already exists."
    cleanup_policy_versions "${CLAIM_POLICY_NAME}"
    echo "  Creating new default version..."
    aws iot create-policy-version \
        --policy-name "${CLAIM_POLICY_NAME}" \
        --policy-document "file://${CLAIM_POLICY_FILE}" \
        --set-as-default \
        --region "${AWS_REGION}" >/dev/null 2>&1 || \
    echo "  Warning: Could not update policy. Using existing."
else
    aws iot create-policy \
        --policy-name "${CLAIM_POLICY_NAME}" \
        --policy-document "file://${CLAIM_POLICY_FILE}" \
        --region "${AWS_REGION}" >/dev/null
    echo "  Policy '${CLAIM_POLICY_NAME}' created."
fi

#------------------------------------------------------------------------------
# Step 10: Attach claim policy to claim certificate
#------------------------------------------------------------------------------
echo ""
echo "[Step 11/12] Attaching policy to claim certificate..."
aws iot attach-policy \
    --policy-name "${CLAIM_POLICY_NAME}" \
    --target "${CERT_ARN}" \
    --region "${AWS_REGION}"
echo "  Policy '${CLAIM_POLICY_NAME}' attached to certificate."

###############################################################################
# PHASE 3: Verification & Output
###############################################################################

#------------------------------------------------------------------------------
# Step 11: Verify setup
#------------------------------------------------------------------------------
echo ""
echo "[Step 12/12] Verifying setup..."
echo "  IAM Role: $(aws iam get-role --role-name ${PROVISIONING_ROLE_NAME} --query 'Role.Arn' --output text)"
echo "  Provisioning Template: $(aws iot describe-provisioning-template --template-name ${PROVISIONING_TEMPLATE_NAME} --region ${AWS_REGION} --query 'templateName' --output text)"
echo "  Claim Policy: $(aws iot get-policy --policy-name ${CLAIM_POLICY_NAME} --region ${AWS_REGION} --query 'policyName' --output text)"
echo "  Device Policy: $(aws iot get-policy --policy-name ${DEVICE_POLICY_NAME} --region ${AWS_REGION} --query 'policyName' --output text)"
echo "  Verification complete."

#------------------------------------------------------------------------------
# Generate C header with credentials for firmware embedding
#------------------------------------------------------------------------------
echo ""
echo "Generating C header for firmware..."

# Extract the raw 32-byte ECC private key scalar as a C hex array
RAW_KEY_HEX=$(openssl ec -in "${CLAIM_KEY}" -noout -text 2>/dev/null | \
    sed -n '/priv:/,/pub:/p' | \
    grep -v 'priv:\|pub:' | \
    tr -d ' :\n')
RAW_KEY_FILE="${OUTPUT_DIR}/claim_private_key_raw.bin"
python -c "import sys; open('${RAW_KEY_FILE}','wb').write(bytes.fromhex('${RAW_KEY_HEX}'))"
echo "  Raw private key (32 bytes) saved: ${RAW_KEY_FILE}"

# Format as C array
RAW_KEY_C_ARRAY=$(echo -n "${RAW_KEY_HEX}" | fold -w2 | awk '
    BEGIN { line="" ; count=0 }
    {
        if (count > 0 && count % 8 == 0) { printf "%s\n", line; line="" }
        if (line == "") { line = "    0x" toupper($0) }
        else { line = line ", 0x" toupper($0) }
        count++
    }
    END { if (line != "") print line }
' | sed 's/$/,/' | sed '$ s/,$//')

# Convert PEM to C string
cert_c_str=$(awk '{printf "    \"%s\\n\"\\\n", $0}' "${CLAIM_CERT}")
key_c_str=$(awk '{printf "    \"%s\\n\"\\\n", $0}' "${CLAIM_KEY}")
rootca_c_str=$(awk '{printf "    \"%s\\n\"\\\n", $0}' "${ROOT_CA}")

cat > "${C_HEADER}" << HEADER_EOF
/******************************************************************************
 * aws_claim_credentials.h
 *
 * AUTO-GENERATED — do not edit manually.
 * Generated on: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
 *
 * Claim certificate credentials for AWS IoT Fleet Provisioning.
 * These are embedded in firmware for initial provisioning only.
 ******************************************************************************/
#ifndef AWS_CLAIM_CREDENTIALS_H
#define AWS_CLAIM_CREDENTIALS_H

/* AWS IoT endpoint */
#define AWS_IOT_ENDPOINT    "${IOT_ENDPOINT}"

/* Provisioning template name */
#define PROVISIONING_TEMPLATE_NAME  "${PROVISIONING_TEMPLATE_NAME}"

/* Amazon Root CA 1 */
#define AWS_ROOT_CA_PEM \\
${rootca_c_str}
    ""

/* Claim Certificate (PEM) */
#define AWS_CLAIM_CERTIFICATE_PEM \\
${cert_c_str}
    ""

/* Claim Private Key (PEM) */
#define AWS_CLAIM_PRIVATE_KEY_PEM \\
${key_c_str}
    ""

/* Raw ECC P-256 private key (32 bytes) for PSA Crypto import */
unsigned char aws_client_key[] = {
${RAW_KEY_C_ARRAY}
};
const size_t aws_client_key_size = sizeof(aws_client_key);

#endif /* AWS_CLAIM_CREDENTIALS_H */
HEADER_EOF

echo "  C header saved: ${C_HEADER}"

#------------------------------------------------------------------------------
# Summary
#------------------------------------------------------------------------------
echo ""
echo "============================================================"
echo " SETUP COMPLETE"
echo "============================================================"
echo ""
echo " Phase 1 - AWS Infrastructure:"
echo "   IAM Role ARN : ${PROVISIONING_ROLE_ARN}"
echo "   Device Policy: ${DEVICE_POLICY_NAME}"
echo "   Template     : ${PROVISIONING_TEMPLATE_NAME}"
echo ""
echo " Phase 2 - Claim Certificate:"
echo "   Certificate ARN: ${CERT_ARN}"
echo "   Claim Policy   : ${CLAIM_POLICY_NAME}"
echo "   IoT Endpoint   : ${IOT_ENDPOINT}"
echo ""
echo " Files generated in ${OUTPUT_DIR}/:"
echo "   trust_policy.json          - IAM trust policy for IoT service"
echo "   role_policy.json           - IAM role permissions policy"
echo "   device_policy.json         - Device IoT policy document"
echo "   provisioning_template.json - Fleet provisioning template body"
echo "   claim_private.pem.key      - Claim private key (PEM)"
echo "   claim_private_key_raw.bin  - Raw 32-byte private key scalar"
echo "   claim_certificate.pem.crt  - Claim certificate (active)"
echo "   claim.csr                  - Certificate signing request"
echo "   AmazonRootCA1.pem          - Amazon Root CA"
echo "   claim_cert_arn.txt         - Certificate ARN"
echo "   claim_policy.json          - Claim IoT policy document"
echo "   aws_claim_credentials.h    - C header for firmware"
echo ""
echo " Next steps:"
echo "   1. Copy credentials from aws_claim_credentials.h into mqtt_client_config.h"
echo "   2. Build and flash the firmware"
echo "   3. Device uses claim cert to connect and run Fleet Provisioning"
echo "============================================================"
