/* FreeRTOS Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* coreMQTT library includes. */
#include "core_mqtt.h"

/* Backoff Algorithm library includes. */
#include "backoff_algorithm.h"

/* Transport interface implementation includes. */
#include "transport_plaintext.h"

/* Demo configuration includes. */
#include "demo_config.h"

/* Defines */
/*-----------------------------------------------------------*/

/* Compile time error for undefined configs. */
#ifndef democonfigMQTT_BROKER_ENDPOINT
    #error "Define the config democonfigMQTT_BROKER_ENDPOINT by following the instructions in file demo_config.h."
#endif

/*-----------------------------------------------------------*/

/* Default values for configs. */
#ifndef democonfigCLIENT_IDENTIFIER

/**
 * @brief The MQTT client identifier used in this example.  Each client identifier
 * must be unique so edit as required to ensure no two clients connecting to the
 * same broker use the same client identifier.
 *
 * @note Appending __TIME__ to the client id string will help to create a unique
 * client id every time an application binary is built. Only a single instance of
 * this application's compiled binary may be used at a time, since the client ID
 * will always be the same.
 */
    #define democonfigCLIENT_IDENTIFIER    "testClient"__TIME__
#endif

#ifndef democonfigMQTT_BROKER_PORT

/**
 * @brief The port to use for the demo.
 */
    #define democonfigMQTT_BROKER_PORT    ( 8883 )
#endif

/*-----------------------------------------------------------*/

/**
 * @brief The maximum number of retries for network operation with server.
 */
#define mqttexampleRETRY_MAX_ATTEMPTS                     ( 5U )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying failed operation
 *  with server.
 */
#define mqttexampleRETRY_MAX_BACKOFF_DELAY_MS             ( 5000U )

/**
 * @brief The base back-off delay (in milliseconds) to use for network operation retry
 * attempts.
 */
#define mqttexampleRETRY_BACKOFF_BASE_MS                  ( 500U )

/**
 * @brief Timeout for receiving CONNACK packet in milliseconds.
 */
#define mqttexampleCONNACK_RECV_TIMEOUT_MS                ( 1000U )

/**
 * @brief The prefix to the topic(s) subscribe(d) to and publish(ed) to in the example.
 *
 * The topic name starts with the client identifier to ensure that each demo
 * interacts with a unique topic name.
 */
#define mqttexampleTOPIC_PREFIX                           democonfigCLIENT_IDENTIFIER "/example/topic"

/**
 * @brief The number of topic filters to subscribe.
 */
#define mqttexampleTOPIC_COUNT                            ( 3 )

/**
 * @brief The size of the buffer for each topic string.
 */
#define mqttexampleTOPIC_BUFFER_SIZE                      ( 100U )

/**
 * @brief The MQTT message published in this example.
 */
#define mqttexampleMESSAGE                                "Hello World!"

/**
 * @brief Time in ticks to wait between each cycle of the demo implemented
 * by prvMQTTDemoTask().
 */
#define mqttexampleDELAY_BETWEEN_DEMO_ITERATIONS_TICKS    ( pdMS_TO_TICKS( 5000U ) )

/**
 * @brief Timeout for MQTT_ProcessLoop in milliseconds.
 */
#define mqttexamplePROCESS_LOOP_TIMEOUT_MS                ( 500U )

/**
 * @brief The keep-alive timeout period reported to the broker while establishing
 * an MQTT connection.
 *
 * It is the responsibility of the client to ensure that the interval between
 * control packets being sent does not exceed this keep-alive value. In the
 * absence of sending any other control packets, the client MUST send a
 * PINGREQ packet.
 */
#define mqttexampleKEEP_ALIVE_TIMEOUT_SECONDS             ( 60U )

/**
 * @brief Delay (in ticks) between consecutive cycles of MQTT publish operations in a
 * demo iteration.
 *
 * Note that the process loop also has a timeout, so the total time between
 * publishes is the sum of the two delays.
 */
#define mqttexampleDELAY_BETWEEN_PUBLISHES_TICKS          ( pdMS_TO_TICKS( 2000U ) )

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS         ( 200U )

/**
 * @brief The length of the outgoing publish records array used by the coreMQTT
 * library to track QoS > 0 packet ACKS for outgoing publishes.
 */
#define mqttexampleOUTGOING_PUBLISH_RECORD_LEN            ( 10U )

/**
 * @brief The length of the incoming publish records array used by the coreMQTT
 * library to track QoS > 0 packet ACKS for incoming publishes.
 */
#define mqttexampleINCOMING_PUBLISH_RECORD_LEN            ( 10U )

/**
 * @brief Milliseconds per second.
 */
#define MILLISECONDS_PER_SECOND                           ( 1000U )

/**
 * @brief Milliseconds per FreeRTOS tick.
 */
#define MILLISECONDS_PER_TICK                             ( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ )

/*-----------------------------------------------------------*/

/* Structs */

/**
 * @brief Each compilation unit that consumes the NetworkContext must define it.
 * It should contain a single pointer to the type of your desired transport.
 * When using multiple transports in the same compilation unit, define this pointer as void *.
 *
 * @note Transport stacks are defined in FreeRTOS-Plus/Source/Application-Protocols/network_transport.
 */
struct NetworkContext
{
    PlaintextTransportParams_t * pParams;
};

/* Static Function Declarations */

/* Static Global Variables */

/* Static Functions */
static BaseType_t prvInitializeDemo( void )
{
    return;
}

static BaseType_t prvConnectToServerWithBackoffRetries( NetworkContext_t * pxNetworkContext )
{
    BaseType_t xStatus = pdFAIL;
    PlaintextTransportStatus_t xNetworkStatus;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t xReconnectParams;
    uint16_t usNextRetryBackOff = 0U;

    /* Initialize reconnect attempts and interval.*/
    BackoffAlgorithm_InitializeParams( &xReconnectParams,
                                       mqttexampleRETRY_BACKOFF_BASE_MS,
                                       mqttexampleRETRY_MAX_BACKOFF_DELAY_MS,
                                       mqttexampleRETRY_MAX_ATTEMPTS );

    /* Attempt to connect to MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase till maximum
     * attempts are reached.
     */
    do
    {
        /* Establish a TCP connection with the MQTT broker. This example connects to
         * the MQTT broker as specified in democonfigMQTT_BROKER_ENDPOINT and
         * democonfigMQTT_BROKER_PORT at the top of this file. */
        LogInfo( ( "Connecting to %s:%d over PlainText.",
                   democonfigMQTT_BROKER_ENDPOINT,
                   democonfigMQTT_BROKER_PORT ) );
        xNetworkStatus = Plaintext_FreeRTOS_Connect( pxNetworkContext,
                                                     democonfigMQTT_BROKER_ENDPOINT,
                                                     democonfigMQTT_BROKER_PORT,
                                                     mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS,
                                                     mqttexampleTRANSPORT_SEND_RECV_TIMEOUT_MS );

        if( xNetworkStatus != PLAINTEXT_TRANSPORT_SUCCESS )
        {
            /* Generate a random number and calculate backoff value (in milliseconds) for
             * the next connection retry.
             * Note: It is recommended to seed the random number generator with a device-specific
             * entropy source so that possibility of multiple devices retrying failed network operations
             * at similar intervals can be avoided. */
            xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &xReconnectParams, uxRand(), &usNextRetryBackOff );

            if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
                LogError( ( "Failed to connect to %s:%d. All attempts exhausted.",
                            democonfigMQTT_BROKER_ENDPOINT,
                            democonfigMQTT_BROKER_PORT ) );
            }
            else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
            {
                LogError( ( "Failed to connect to %s:%d. Retrying to connect with backoff and jitter.",
                            democonfigMQTT_BROKER_ENDPOINT,
                            democonfigMQTT_BROKER_PORT ) );
                vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );
            }
        }
    } while( ( xNetworkStatus != PLAINTEXT_TRANSPORT_SUCCESS ) && ( xBackoffAlgStatus == BackoffAlgorithmSuccess ) );

    if( xNetworkStatus == PLAINTEXT_TRANSPORT_SUCCESS )
    {
        xStatus = pdPASS;
    }

    return xStatus;
}

static BaseType_t prvEstablishMqttConnectionWithBroker( MQTTContext_t * pxMQTTContext,
                                                        NetworkContext_t * pxNetworkContext )
{
    BaseType_t xStatus = pdFAIL;

    return xStatus;
}

static BaseType_t prvSubscribeToTopics( MQTTContext_t * pxMQTTContext,
                                        const char * pTopicList,
                                        uint32_t ulTopicListLength )
{
    BaseType_t xStatus = pdFAIL;

    return xStatus;
}

static BaseType_t prvUnsubscribeFromTopics( MQTTContext_t * pxMQTTContext,
                                            const char * pTopicList,
                                            uint32_t ulTopicListLength )
{
    BaseType_t xStatus = pdFAIL;

    return xStatus;
}

static BaseType_t prvPublishToTopic( MQTTContext_t * pxMQTTContext,
                                     const char * pcTopicName,
                                     const char * pcMessage )
{
    BaseType_t xStatus = pdFAIL;

    return xStatus;
}

static void prvMQTTDemoTask( void * pvParameters )
{
    /* Networking contexts. */
    NetworkContext_t xNetworkContext = { 0 };
    MQTTContext_t xMQTTContext = { 0 };
    PlaintextTransportParams_t xPlaintextTransportParams = { 0 };

    /* Demo status. */
    BaseType_t xDemoStatus = pdFAIL;

    xNetworkContext.pParams = &xPlaintextTransportParams;

    /* Suppress compiler warnings for unused variable. */
    ( void ) pvParameters;

    for( ; ; )
    {
        LogInfo( ( "---------STARTING DEMO---------\r\n" ) );

        prvInitializeDemo();

        LogInfo( ( "Waiting for the network link up event..." ) );

        while( xPlatformIsNetworkUp() == pdFALSE )
        {
            vTaskDelay( pdMS_TO_TICKS( 1000U ) );
        }

        xDemoStatus = prvConnectToServerWithBackoffRetries( &xNetworkContext );

        if( xDemoStatus == pdPASS )
        {
            xDemoStatus = prvEstablishMqttConnectionWithBroker( &xMQTTContext, &xNetworkContext );
        }

        if( xDemoStatus == pdPASS )
        {
            LogInfo( ( "Demo completed successfully.\r\n" ) );
            LogInfo( ( "-------DEMO FINISHED-------\r\n" ) );;
            break;
        }
        else
        {
            LogInfo( ( "Short delay before starting the next iteration.... \r\n\r\n" ) );
            vTaskDelay( mqttexampleDELAY_BETWEEN_DEMO_ITERATIONS_TICKS )
        }
    }

    vTaskDelete( NULL );
}

/* Public Functions */
void vStartSimpleMQTTDemo( void )
{
    BaseType_t xStatus;

    xStatus = xTaskCreate( prvMQTTDemoTask,          /* Function that implements the task. */
                           "MQTTDemoTask",           /* Text name for the task - only used for debugging. */
                           democonfigDEMO_STACKSIZE, /* Size of stack (in words, not bytes) to allocate for the task. */
                           NULL,                     /* Task parameter - not used in this case. */
                           tskIDLE_PRIORITY,         /* Task priority, must be between 0 and configMAX_PRIORITIES - 1. */
                           NULL );                   /* Used to pass out a handle to the created task - not used in this case. */

    if( xStatus != pdPASS )
    {
        LogError( ( "Failed to create the demo task." ) );
    }
}
