/******************************************************************************
 * aws_claim_credentials.h
 *
 * AUTO-GENERATED — do not edit manually.
 * Generated on: 2026-10-08 09:50:30 UTC
 *
 * Claim certificate credentials for AWS IoT Fleet Provisioning.
 * These are embedded in firmware for initial provisioning only.
 ******************************************************************************/
#ifndef AWS_CLAIM_CREDENTIALS_H
#define AWS_CLAIM_CREDENTIALS_H

/* AWS IoT endpoint */
#define AWS_IOT_ENDPOINT    "a3fbo5uaq4rhje-ats.iot.us-east-1.amazonaws.com"

/* Provisioning template name */
#define PROVISIONING_TEMPLATE_NAME  "FleetProvisioningTemplate"

/* Amazon Root CA 1 */
#define AWS_ROOT_CA_PEM \
    "-----BEGIN CERTIFICATE-----\n"\
    "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"\
    "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"\
    "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"\
    "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"\
    "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"\
    "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"\
    "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"\
    "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"\
    "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"\
    "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"\
    "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"\
    "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"\
    "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"\
    "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"\
    "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"\
    "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"\
    "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"\
    "rqXRfboQnoZsG4q5WTP468SQvvG5\n"\
    "-----END CERTIFICATE-----\n"\
    ""

/* Claim Certificate (PEM) */
#define AWS_CLAIM_CERTIFICATE_PEM \
    "-----BEGIN CERTIFICATE-----\n"\
    "MIICoTCCAYmgAwIBAgIUdJuNtI8oSUzAfomyrfS11zTIRo8wDQYJKoZIhvcNAQEL\n"\
    "BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n"\
    "SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MTAwODA5NDgx\n"\
    "NVoXDTQ5MTIzMTIzNTk1OVowMTESMBAGA1UEAwwJQ2xhaW1DZXJ0MQ4wDAYDVQQK\n"\
    "DAVNeU9yZzELMAkGA1UEBhMCVVMwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAT7\n"\
    "rPpu8b1OMer440CQAXe5KbzU4hH79pPnI5zUdZshj2pNcygv0RSFKxVSzsqSyJv+\n"\
    "AYVcUJwe9hWLLAirrw4Zo2AwXjAfBgNVHSMEGDAWgBT5xYzy4lZ15AhUVZCitfEq\n"\
    "knb9VTAdBgNVHQ4EFgQUPi6ERAJyA5LoLUYG+9WejO7cdxEwDAYDVR0TAQH/BAIw\n"\
    "ADAOBgNVHQ8BAf8EBAMCB4AwDQYJKoZIhvcNAQELBQADggEBAHUo+AY8arBhY8ls\n"\
    "if/rW+Cq8XaUxzNeIxvFUnEAJlHNjK2sBMocjbATHoXXnE58/DfR1QSvg9TZEqOC\n"\
    "1zScr6VfWV1Jt1PImhELZWKD6f8d2vA9aa9+K3HxNvsMBEjfvJUTYm9TJENpKbne\n"\
    "4HlCAQDzeWjvJPuPmTd8hvqJyTlA1p2UYuykkTi0P0YW+d/UQvL+eK/jckh3mYV6\n"\
    "q1p9wDPbPJkpf7lnD9F27urlkqVhGaTwkx9F5nRINuk3GEEb3fSN0X6XLsE2uqk0\n"\
    "2KAw5wEJdLRukNjWodR80jOBfjXJExp/l/YHNpVHnPohD7CEzONKpBBfYLTXiaWd\n"\
    "nMGoV/c=\n"\
    "-----END CERTIFICATE-----\n"\
    ""

/* Claim Private Key (PEM) */
#define AWS_CLAIM_PRIVATE_KEY_PEM \
    "-----BEGIN EC PRIVATE KEY-----\n"\
    "MHcCAQEEIKb6aSHJ7qZwSsjtKzjv5Tkb88Mx0lJJRfOtBFcjpvQxoAoGCCqGSM49\n"\
    "AwEHoUQDQgAE+6z6bvG9TjHq+ONAkAF3uSm81OIR+/aT5yOc1HWbIY9qTXMoL9EU\n"\
    "hSsVUs7Kksib/gGFXFCcHvYViywIq68OGQ==\n"\
    "-----END EC PRIVATE KEY-----\n"\
    ""

/* Raw ECC P-256 private key (32 bytes) for PSA Crypto import */
unsigned char aws_client_key[] = {
    0xA6, 0xFA, 0x69, 0x21, 0xC9, 0xEE, 0xA6, 0x70,
    0x4A, 0xC8, 0xED, 0x2B, 0x38, 0xEF, 0xE5, 0x39,
    0x1B, 0xF3, 0xC3, 0x31, 0xD2, 0x52, 0x49, 0x45,
    0xF3, 0xAD, 0x04, 0x57, 0x23, 0xA6, 0xF4, 0x31
};
const size_t aws_client_key_size = sizeof(aws_client_key);

#endif /* AWS_CLAIM_CREDENTIALS_H */
