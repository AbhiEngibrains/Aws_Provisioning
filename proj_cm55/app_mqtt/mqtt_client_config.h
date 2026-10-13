/******************************************************************************
* File Name:   mqtt_client_config.h
*
* Description: This file contains all the configuration macros used by the
*              MQTT client in this example.
*
* Related Document: See README.md
*
*
*******************************************************************************
# \copyright
# (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG.  SPDX-License-Identifier: Apache-2.0
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*******************************************************************************/

#ifndef MQTT_CLIENT_CONFIG_H_
#define MQTT_CLIENT_CONFIG_H_

#include "cy_mqtt_api.h"

/*******************************************************************************
* Macros
********************************************************************************/

/***************** MQTT CLIENT CONNECTION CONFIGURATION MACROS *****************/
/* MQTT Broker/Server address and port used for the MQTT connection. */
#define MQTT_BROKER_ADDRESS               "a3fbo5uaq4rhje-ats.iot.us-east-1.amazonaws.com"
#define MQTT_PORT                         8883

/* Set this macro to 1 if a secure (TLS) connection to the MQTT Broker is
 * required to be established, else 0.
 */
#define MQTT_SECURE_CONNECTION            ( 1 )

/* Configure the user credentials to be sent as part of MQTT CONNECT packet.
 * Note: AWS IoT Core uses certificate-based authentication, not username/password.
 * Leave these empty for AWS IoT connections.
 */
#define MQTT_USERNAME                     ""
#define MQTT_PASSWORD                     ""


/********************* MQTT MESSAGE CONFIGURATION MACROS **********************/
/* The MQTT topics to be used by the publisher and subscriber. */
#define MQTT_PUB_TOPIC                    "ledstatus"
#define MQTT_SUB_TOPIC                    "ledstatus"

/* Set the QoS that is associated with the MQTT publish, and subscribe messages.
 * Valid choices are 0, 1, and 2. Other values should not be used in this macro.
 */
#define MQTT_MESSAGES_QOS                 ( 1 )

/* Configuration for the 'Last Will and Testament (LWT)'. It is an MQTT message
 * that will be published by the MQTT broker if the MQTT connection is
 * unexpectedly closed. This configuration is sent to the MQTT broker during
 * MQTT connect operation and the MQTT broker will publish the Will message on
 * the Will topic when it recognizes an unexpected disconnection from the client.
 *
 * If you want to use the last will message, set this macro to 1 and configure
 * the topic and will message, else 0.
 */
#define ENABLE_LWT_MESSAGE                ( 0 )
#if ENABLE_LWT_MESSAGE
    #define MQTT_WILL_TOPIC_NAME          MQTT_PUB_TOPIC "/will"
    #define MQTT_WILL_MESSAGE             ("MQTT client unexpectedly disconnected!")
#endif

/* MQTT messages which are published on the MQTT_PUB_TOPIC that controls the
 * device (user LED in this example) state in this code example.
 */
#define MQTT_DEVICE_ON_MESSAGE            "TURN ON"
#define MQTT_DEVICE_OFF_MESSAGE           "TURN OFF"


/******************* OTHER MQTT CLIENT CONFIGURATION MACROS *******************/
/* A unique client identifier to be used for every MQTT connection. */
#define MQTT_CLIENT_IDENTIFIER            "psocedge-mqtt-client"

/* The timeout in milliseconds for MQTT operations in this example. */
#define MQTT_TIMEOUT_MS                   ( 5000 )

/* The keep-alive interval in seconds used for MQTT ping request. */
#define MQTT_KEEP_ALIVE_SECONDS           ( 60 )

/* Every active MQTT connection must have a unique client identifier. If you
 * are using the above 'MQTT_CLIENT_IDENTIFIER' as client ID for multiple MQTT
 * connections simultaneously, set this macro to 1. The device will then
 * generate a unique client identifier by appending a timestamp to the
 * 'MQTT_CLIENT_IDENTIFIER' string. Example: 'psoc6-mqtt-client5927'
 */
#define GENERATE_UNIQUE_CLIENT_ID         ( 1 )

/* The longest client identifier that an MQTT server must accept (as defined
 * by the MQTT 3.1.1 spec) is 23 characters. However some MQTT brokers support
 * longer client IDs. Configure this macro as per the MQTT broker specification.
 */
#define MQTT_CLIENT_IDENTIFIER_MAX_LEN    ( 23 )

/* As per Internet Assigned Numbers Authority (IANA) the port numbers assigned
 * for MQTT protocol are 1883 for non-secure connections and 8883 for secure
 * connections. In some cases there is a need to use other ports for MQTT like
 * port 443 (which is reserved for HTTPS). Application Layer Protocol
 * Negotiation (ALPN) is an extension to TLS that allows many protocols to be
 * used over a secure connection. The ALPN ProtocolNameList specifies the
 * protocols that the client would like to use to communicate over TLS.
 *
 * This macro specifies the ALPN Protocol Name to be used that is supported
 * by the MQTT broker in use.
 * Note: For AWS IoT, currently "x-amzn-mqtt-ca" is the only supported ALPN
 *       ProtocolName and it is only supported on port 443.
 *
 * Uncomment the below line and specify the ALPN Protocol Name to use this
 * feature.
 */
// #define MQTT_ALPN_PROTOCOL_NAME           "x-amzn-mqtt-ca"

/* Server Name Indication (SNI) is extension to the Transport Layer Security
 * (TLS) protocol. As required by some MQTT Brokers, SNI typically includes the
 * hostname in the Client Hello message sent during TLS handshake.
 *
 * Uncomment the below line and specify the SNI Host Name to use this extension
 * as specified by the MQTT Broker.
 */
// #define MQTT_SNI_HOSTNAME                 "SNI_HOST_NAME"

/* A Network buffer is allocated for sending and receiving MQTT packets over
 * the network. Specify the size of this buffer using this macro.
 *
 * Note: The minimum buffer size is defined by 'CY_MQTT_MIN_NETWORK_BUFFER_SIZE'
 * macro in the MQTT library. Please ensure this macro value is larger than
 * 'CY_MQTT_MIN_NETWORK_BUFFER_SIZE'.
 *
 * For Fleet Provisioning, the certificate response from AWS IoT can be
 * ~1500-2000 bytes. The buffer must be large enough to hold the full
 * MQTT PUBLISH packet including topic + payload + overhead.
 */
#define MQTT_NETWORK_BUFFER_SIZE          ( 5 * 1024 )

/* Maximum MQTT connection re-connection limit. */
#define MAX_MQTT_CONN_RETRIES            (150u)

/* MQTT re-connection time interval in milliseconds. */
#define MQTT_CONN_RETRY_INTERVAL_MS      (2000)


/**************** MQTT CLIENT CERTIFICATE CONFIGURATION MACROS ****************/

/* Configure the below credentials in case of a secure MQTT connection. */
/* PEM-encoded client certificate.
 * IMPORTANT: The certificate and private key MUST be a matching pair.
 * Generate them together using aws_claim_cert_setup.sh, then paste here.
 * Verify with:
 *   openssl x509 -in cert.pem -noout -pubkey | openssl md5
 *   openssl ec -in key.pem -pubout | openssl md5
 * Both hashes must match.
 */
#define CLAIM_CERTIFICATE      \
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


/* PEM-encoded client private key - MUST match CLIENT_CERTIFICATE above */
#define CLAIM_PRIVATE_KEY          \
     "-----BEGIN EC PRIVATE KEY-----\n"\
    "MHcCAQEEIKb6aSHJ7qZwSsjtKzjv5Tkb88Mx0lJJRfOtBFcjpvQxoAoGCCqGSM49\n"\
    "AwEHoUQDQgAE+6z6bvG9TjHq+ONAkAF3uSm81OIR+/aT5yOc1HWbIY9qTXMoL9EU\n"\
    "hSsVUs7Kksib/gGFXFCcHvYViywIq68OGQ==\n"\
    "-----END EC PRIVATE KEY-----\n"\
    ""



/* PEM-encoded Root CA certificate */
#define ROOT_CA_CERTIFICATE     \
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
/* Raw ECC private key bytes for the claim certificate
 */
#define CLAIM_KEY_DATA \
    0xA6, 0xFA, 0x69, 0x21, 0xC9, 0xEE, 0xA6, 0x70,\
    0x4A, 0xC8, 0xED, 0x2B, 0x38, 0xEF, 0xE5, 0x39,\
    0x1B, 0xF3, 0xC3, 0x31, 0xD2, 0x52, 0x49, 0x45,\
    0xF3, 0xAD, 0x04, 0x57, 0x23, 0xA6, 0xF4, 0x31
/******************************************************************************
* Global Variables
*******************************************************************************/
extern cy_mqtt_broker_info_t broker_info;
extern cy_awsport_ssl_credentials_t  *security_info;
extern cy_mqtt_connect_info_t connection_info;


#endif /* MQTT_CLIENT_CONFIG_H_ */
