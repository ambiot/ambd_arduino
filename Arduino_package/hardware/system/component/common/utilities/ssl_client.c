#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

#include "platform_opts.h"
#include "osdep_service.h"

#if CONFIG_USE_POLARSSL

#include "polarssl/config.h"
#include "polarssl/net.h"
#include "polarssl/ssl.h"
#include "polarssl/error.h"
#include "polarssl/memory.h"

#define SERVER_PORT   443
#define SERVER_HOST   "192.168.13.15"
#define GET_REQUEST   "GET / HTTP/1.0\r\n\r\n"
#define DEBUG_LEVEL   0

//#define SSL_CLIENT_EXT
#ifdef SSL_CLIENT_EXT
#define STACKSIZE     2048
#else
#define STACKSIZE     1150
#endif

static int is_task = 0;
static char server_host[16];
static size_t min_heap_size = 0;

static void my_debug(void *ctx, int level, const char *str)
{
	if(level <= DEBUG_LEVEL) {
		printf("\n\r%s", str);
	}
}

static int my_random(void *p_rng, unsigned char *output, size_t output_len)
{
	rtw_get_random_bytes(output, output_len);
	return 0;
}

void* my_malloc(size_t size)
{
	void *ptr = pvPortMalloc(size);
	size_t current_heap_size = xPortGetFreeHeapSize();

	if((current_heap_size < min_heap_size) || (min_heap_size == 0))
		min_heap_size = current_heap_size;

	return ptr;
}

#define my_free		vPortFree

static void ssl_client(void *param)
{
	int ret, len, server_fd = -1;
	unsigned char buf[512];
	ssl_context ssl;
	int retry_count = 0;

	memory_set_own(my_malloc, my_free);

	/*
	 * 1. Start the connection
	 */
	printf("\n\r  . Connecting to tcp/%s/%d...", server_host, SERVER_PORT);

	if((ret = net_connect(&server_fd, server_host, SERVER_PORT)) != 0) {
		printf(" failed\n\r  ! net_connect returned %d\n", ret);
		goto exit1;
	}

	printf(" ok\n");

	/*
	 * 2. Setup stuff
	 */
	printf("\n\r  . Setting up the SSL/TLS structure..." );

	if((ret = ssl_init(&ssl)) != 0) {
		printf(" failed\n\r  ! ssl_init returned %d\n", ret);
		goto exit;
	}
#ifdef SSL_CLIENT_EXT
	if((ret = ssl_client_ext_init()) != 0) {
		printf(" failed\n\r  ! ssl_client_ext_init returned %d\n", ret);
		goto exit;
	}
#endif
	ssl_set_endpoint(&ssl, SSL_IS_CLIENT);
	ssl_set_authmode(&ssl, SSL_VERIFY_NONE);
	ssl_set_rng(&ssl, my_random, NULL);
	ssl_set_bio(&ssl, net_recv, &server_fd, net_send, &server_fd);
	ssl_set_dbg(&ssl, my_debug, NULL);
#ifdef POLARSSL_DEBUG_C
	debug_set_threshold(DEBUG_LEVEL);
#endif
#ifdef SSL_CLIENT_EXT
	if((ret = ssl_client_ext_setup(&ssl)) != 0) {
		printf(" failed\n\r  ! ssl_client_ext_setup returned %d\n", ret);
		goto exit;
	}
#endif

	printf(" ok\n");

	/*
	 * 3. Handshake
	 */
	printf("\n\r  . Performing the SSL/TLS handshake...");

	while((ret = ssl_handshake(&ssl)) != 0) {
		if((ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE  
			&& ret != POLARSSL_ERR_NET_RECV_FAILED) || retry_count >= 5) {
			printf(" failed\n\r  ! ssl_handshake returned -0x%x\n", -ret);
			goto exit;
		}
		retry_count++;
	}

	printf(" ok\n");
	printf("\n\r  . Use ciphersuite %s\n", ssl_get_ciphersuite(&ssl));

	/*
	 * 4. Write the GET request
	 */
	printf("\n\r  > Write to server:");

	len = sprintf((char *) buf, GET_REQUEST);

	while((ret = ssl_write(&ssl, buf, len)) <= 0) {
		if(ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE) {
			printf(" failed\n\r  ! ssl_write returned %d\n", ret);
			goto exit;
		}
	}

	len = ret;
	printf(" %d bytes written\n\r\n\r%s\n", len, (char *) buf);

	/*
	 * 5. Read the HTTP response
	 */
	printf("\n\r  < Read from server:");

	do {
		len = sizeof(buf) - 1;
		memset(buf, 0, sizeof(buf));
		ret = ssl_read(&ssl, buf, len);

		if(ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE)
			continue;

		if(ret == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY)
			break;

		if(ret < 0) {
			printf(" failed\n\r  ! ssl_read returned %d\n", ret);
			break;
		}

		if(ret == 0) {
			printf("\n\rEOF\n");
			break;
		}

		len = ret;
		printf(" %d bytes read\n\r\n\r%s\n", len, (char *) buf);
	}
	while(1);

	ssl_close_notify(&ssl);

exit:

#ifdef POLARSSL_ERROR_C
	if(ret != 0) {
		char error_buf[100];
		polarssl_strerror(ret, error_buf, 100);
		printf("\n\rLast error was: %d - %s\n", ret, error_buf);
	}
#endif

	net_close(server_fd);
	ssl_free(&ssl);
#ifdef SSL_CLIENT_EXT
	ssl_client_ext_free();
#endif
exit1:

	if(is_task) {
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
		printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif

		if(min_heap_size > 0)
			printf("\n\rMin available heap size = %d bytes during %s\n\r", min_heap_size, __FUNCTION__);

		vTaskDelete(NULL);
	}

	if(param != NULL)
		*((int *) param) = ret;
}

void start_ssl_client(void)
{
	is_task = 1;
	//strcpy(server_host, SERVER_HOST);

	if(xTaskCreate(ssl_client, "ssl_client", STACKSIZE, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate failed", __FUNCTION__);
}

void do_ssl_connect(void)
{
	int ret;
	static int success = 0;
	static int fail = 0;

	is_task = 0;
	strcpy(server_host, SERVER_HOST);
	ssl_client(&ret);

	if(ret != 0)
		printf("\n\r%s fail (success %d times, fail %d times)\n\r", __FUNCTION__, success, ++ fail);
	else
		printf("\n\r%s success (success %d times, fail %d times)\n\r", __FUNCTION__, ++ success, fail);
}

void cmd_ssl_client(int argc, char **argv)
{
	if(argc == 2) {
		strcpy(server_host, argv[1]);
	}
	else {
		printf("\n\rUsage: %s SSL_SERVER_HOST", argv[0]);
		return;
	}

	start_ssl_client();
}

#elif CONFIG_USE_MBEDTLS /* CONFIG_USE_POLARSSL */

#if defined(CONFIG_MBEDTLS_VERSION3) && (CONFIG_MBEDTLS_VERSION3 == 1)
#include "mbedtls/build_info.h"
#include "psa/crypto_types.h"
#include "psa/crypto_values.h"
#include "mbedtls/x509.h"
#else
#include "mbedtls/config.h"
#endif
#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/version.h"

#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1) && defined(CONFIG_SSL_CLIENT_PRIVATE_IN_TZ) && (CONFIG_SSL_CLIENT_PRIVATE_IN_TZ == 1)
#include "device_lock.h"
#endif

#define SERVER_PORT   "443"
#define SERVER_HOST   "192.168.13.15"
#define GET_REQUEST   "GET / HTTP/1.0\r\n\r\n"
#define DEBUG_LEVEL   0

//#define SSL_CLIENT_EXT
#define STACKSIZE     2048

static int is_task = 0;
static char server_host[32];
static size_t min_heap_size = 0;

static void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
	/* To avoid gcc warnings */
	( void ) ctx;
	( void ) level;
	
	printf("\n\r%s:%d: %s\n\r", file, line, str);
}

extern int rtw_get_random_bytes(void* dst, u32 size);
#ifdef SSL_CLIENT_EXT
extern int ssl_client_ext_init(void);
extern int ssl_client_ext_setup(mbedtls_ssl_config *conf);
extern void ssl_client_ext_free(void);
#endif
static int my_random(void *p_rng, unsigned char *output, size_t output_len)
{
	/* To avoid gcc warnings */
	( void ) p_rng;
	
	rtw_get_random_bytes(output, output_len);
	return 0;
}

static void* my_calloc(size_t nelements, size_t elementSize)
{
	size_t current_heap_size, size;
	void *ptr = NULL;

	size = nelements * elementSize;
	ptr = pvPortMalloc(size);

	if(ptr)
		memset(ptr, 0, size);

	current_heap_size = xPortGetFreeHeapSize();

	if((current_heap_size < min_heap_size) || (min_heap_size == 0))
		min_heap_size = current_heap_size;

	return ptr;
}

#define my_free		vPortFree

static void ssl_client(void *param)
{
	int ret, len;
	int retry_count = 0;
	unsigned char buf[512];
	mbedtls_net_context server_fd;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;

#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1) && defined(CONFIG_SSL_CLIENT_PRIVATE_IN_TZ) && (CONFIG_SSL_CLIENT_PRIVATE_IN_TZ == 1)
	rtw_create_secure_context(STACKSIZE*4);
	extern int NS_ENTRY secure_mbedtls_platform_set_calloc_free(void);
	secure_mbedtls_platform_set_calloc_free();
	extern void NS_ENTRY secure_set_ns_device_lock(void (*device_mutex_lock_func)(uint32_t), void (*device_mutex_unlock_func)(uint32_t));
	secure_set_ns_device_lock(device_mutex_lock, device_mutex_unlock);
#endif

	mbedtls_platform_set_calloc_free(my_calloc, my_free);
#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

	/*
	 * 1. Start the connection
	 */
	printf("\n\r  . Connecting to tcp/%s/%s...", server_host, SERVER_PORT);

	mbedtls_net_init(&server_fd);

	if((ret = mbedtls_net_connect(&server_fd, server_host, SERVER_PORT, MBEDTLS_NET_PROTO_TCP)) != 0) {
		printf(" failed\n\r  ! mbedtls_net_connect returned %d\n", ret);
		goto exit1;
	}

	printf(" ok\n");

	/*
	 * 2. Setup stuff
	 */
	printf("  . Setting up the SSL/TLS structure...");

	mbedtls_ssl_init(&ssl);
	mbedtls_ssl_config_init(&conf);

#ifdef SSL_CLIENT_EXT
	if((ret = ssl_client_ext_init()) != 0) {
		printf(" failed\n\r  ! ssl_client_ext_init returned %d\n", ret);
		goto exit;
	}
#endif

	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

	if((ret = mbedtls_ssl_config_defaults(&conf,
		MBEDTLS_SSL_IS_CLIENT,
		MBEDTLS_SSL_TRANSPORT_STREAM,
		MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {

		printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
		goto exit;
	}

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
	mbedtls_ssl_conf_rng(&conf, my_random, NULL);
	mbedtls_ssl_conf_dbg(&conf, my_debug, NULL);

#ifdef SSL_CLIENT_EXT
	if((ret = ssl_client_ext_setup(&conf)) != 0) {
		printf(" failed\n\r  ! ssl_client_ext_setup returned %d\n", ret);
		goto exit;
	}
#endif

	if((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
		printf(" failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret);
		goto exit;
	}

	printf(" ok\n");

	/*
	 * 3. Handshake
	 */
	printf("\n\r  . Performing the SSL/TLS handshake...");

	while((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
		if((ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE
			&& ret != MBEDTLS_ERR_NET_RECV_FAILED) || retry_count >= 5) {

			printf(" failed\n\r  ! mbedtls_ssl_handshake returned -0x%x\n", -ret);
			goto exit;
		}

		retry_count++;
	}

	printf(" ok\n");
	printf("\n\r  . Use ciphersuite %s\n", mbedtls_ssl_get_ciphersuite(&ssl));

	/*
	 * 4. Write the GET request
	 */
	printf("\n\r  > Write to server:");

	len = sprintf((char *) buf, GET_REQUEST);

	while((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0) {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			printf(" failed\n\r  ! mbedtls_ssl_write returned %d\n", ret);
			goto exit;
		}
	}

	len = ret;
	printf(" %d bytes written\n\n%s", len, (char *) buf);

	/*
	 * 5. Read the HTTP response
	 */
	printf("  < Read from server:" );

	do {
		len = sizeof(buf) - 1;
		memset(buf, 0, sizeof(buf));
		ret = mbedtls_ssl_read(&ssl, buf, len);

		if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;

		if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
			break;

		if(ret < 0) {
			printf(" failed\n  ! mbedtls_ssl_read returned %d\n", ret);
			break;
		}

		if(ret == 0) {
			printf("\n\nEOF\n\n");
			break;
		}

		len = ret;
		printf(" %d bytes read\n\n%s", len, (char *) buf);
	}
	while(1);

	mbedtls_ssl_close_notify(&ssl);

exit:

#ifdef MBEDTLS_ERROR_C
	if(ret != 0) {
		char error_buf[100];
		mbedtls_strerror(ret, error_buf, 100);
		printf("Last error was: %d - %s\n\n", ret, error_buf);
	}
#endif

	mbedtls_net_free(&server_fd);
	mbedtls_ssl_free(&ssl);
	mbedtls_ssl_config_free(&conf);

#ifdef SSL_CLIENT_EXT
	ssl_client_ext_free();
#endif

exit1:
	if(is_task) {
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
		printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif

		if(min_heap_size > 0)
			printf("\n\rMin available heap size = %d bytes during %s\n\r", min_heap_size, __FUNCTION__);

		vTaskDelete(NULL);
	}

	if(param != NULL)
		*((int *) param) = ret;
}

#define SERVER_NAME   "www.google.com"
#define GTSR1_CA_PEM                                              \
"-----BEGIN CERTIFICATE-----\r\n"  \
"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\r\n"  \
"bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c\r\n"  \
"-----END CERTIFICATE-----\r\n"
const unsigned char gtsr1_pem[] = GTSR1_CA_PEM;

#if defined(CONFIG_MBEDTLS_VERSION3) && (CONFIG_MBEDTLS_VERSION3 == 1)
static void ssl_client_TLS13(void *param)
{
	printf("\n\r START example %s", __FUNCTION__);
	int retry_count = 0;
	int ret = 0;
	int len, written, frags;
	mbedtls_net_context server_fd;
	unsigned char buf[1024];
	const char *pers = "ssl_client";

	mbedtls_ssl_context ssl;
	mbedtls_ssl_config conf;
	uint32_t flags;
	mbedtls_x509_crt cacert;

#if defined(MBEDTLS_DEBUG_C)
	mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

	psa_status_t status;
	status = psa_crypto_init();
	if( status != 0 ) {
		printf("\r\n psa_crypto_init status = %d", status);
		mbedtls_psa_crypto_free( );
	}

	/*
	 * Make sure memory references are valid.
	 */
	mbedtls_net_init( &server_fd );
	mbedtls_ssl_init( &ssl );
	mbedtls_ssl_config_init( &conf );
	mbedtls_x509_crt_init( &cacert );

	/*
	 * 1. Load the trusted CA
	 */
	printf( "\n\r  . Loading the CA root certificate ..." );
	ret = mbedtls_x509_crt_parse( &cacert, gtsr1_pem, sizeof(gtsr1_pem) );
	if( ret < 0 ) {
		printf( " failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n",
						(unsigned int) -ret );
		goto exit;
	}
	printf( " ok\n" );

	/*
	 * 2. Setup stuff
	 */
	printf( "\n\r  . Setting up the SSL/TLS structure..." );
	if( ( ret = mbedtls_ssl_config_defaults( &conf,
					MBEDTLS_SSL_IS_CLIENT,
					MBEDTLS_SSL_TRANSPORT_STREAM,
					MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 ) {
		printf( " failed\n  ! mbedtls_ssl_config_defaults returned -0x%x\n\n", (unsigned int) -ret );
		goto exit;
	}

	mbedtls_ssl_conf_rng( &conf, my_random, NULL );
	mbedtls_ssl_conf_dbg( &conf, my_debug, NULL );
	mbedtls_ssl_conf_read_timeout( &conf, 0 );
	mbedtls_ssl_conf_session_tickets( &conf, MBEDTLS_SSL_SESSION_TICKETS_ENABLED );
	mbedtls_ssl_conf_tls13_key_exchange_modes( &conf, MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_ALL );
	mbedtls_ssl_conf_renegotiation( &conf, MBEDTLS_SSL_RENEGOTIATION_DISABLED );
	mbedtls_ssl_conf_ca_chain( &conf, &cacert, NULL );
	mbedtls_ssl_conf_min_version( &conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_4 );
	mbedtls_ssl_conf_max_version( &conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_4 );

	if( ( ret = mbedtls_ssl_setup( &ssl, &conf ) ) != 0 ) {
		printf( " failed\n  ! mbedtls_ssl_setup returned -0x%x\n\n", (unsigned int) -ret );
		goto exit;
	}
	if( ( ret = mbedtls_ssl_set_hostname( &ssl, SERVER_NAME ) ) != 0 ) {
		printf( " failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
		goto exit;
	}
	mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
	printf( " ok\n" );

	/*
	 * 3. Start the connection
	 */
	printf("\n\r  . Connecting to tcp/%s/%s...", SERVER_NAME, SERVER_PORT);
	if( ( ret = mbedtls_net_connect( &server_fd,
					   SERVER_NAME, SERVER_PORT, MBEDTLS_NET_PROTO_TCP) ) != 0 ) {
		printf( " failed\n  ! mbedtls_net_connect returned -0x%x\n\n", (unsigned int) -ret );
		goto exit;
	}
	ret = mbedtls_net_set_block( &server_fd );
	if ( ret != 0 ) {
		printf( " failed\n  ! net_set_(non)block() returned -0x%x\n\n", (unsigned int) -ret );
		goto exit;
	}
	printf( " ok\n" );

	/*
	 * 4. Handshake
	 */
	printf( "\n\r  . Performing the SSL/TLS handshake..." );
	while( ( ret = mbedtls_ssl_handshake( &ssl ) ) != 0 ) {
		if( ret != MBEDTLS_ERR_SSL_WANT_READ &&
			ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
			ret != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS ) {
			printf( " failed\n  ! mbedtls_ssl_handshake returned -0x%x\n", (unsigned int) -ret );
			goto exit;
		}
	}
	printf( " ok\n    [ Protocol is %s ]\n    [ Ciphersuite is %s ]\n",
					mbedtls_ssl_get_version( &ssl ),
					mbedtls_ssl_get_ciphersuite( &ssl ) );

	/*
	 * 5. Verify the server certificate
	 */
	printf( "\n\r  . Verifying peer X.509 certificate..." );
	if( ( flags = mbedtls_ssl_get_verify_result( &ssl ) ) != 0 )
		printf( " server sertificate failed\n" );
	else
		printf( " ok\n" );

	/*
	 * 6. Write the GET request
	 */
	printf("\n\r  > Write to server:");

	len = sprintf((char *) buf, GET_REQUEST);

	while((ret = mbedtls_ssl_write(&ssl, buf, len)) <= 0) {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			printf(" failed\n\r  ! mbedtls_ssl_write returned %d\n", ret);
			goto exit;
		}
	}

	len = ret;
	printf(" %d bytes written\n\n%s", len, (char *) buf);

	/*
	 * 7. Read the HTTP response
	 */
	printf("  < Read from server:" );

	do {
		len = sizeof(buf) - 1;
		memset(buf, 0, sizeof(buf));
		ret = mbedtls_ssl_read(&ssl, buf, len);

		if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;

		if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
			break;

		if(ret < 0) {
			printf(" failed\n  ! mbedtls_ssl_read returned %d\n", ret);
			break;
		}

		if(ret == 0) {
			printf("\n\nEOF\n\n");
			break;
		}

		len = ret;
		printf(" %d bytes read\n\n%s", len, (char *) buf);
	}
	while(1);

	mbedtls_ssl_close_notify(&ssl);
	printf( " ok\n" );

exit:
	printf( "\r\n EXIT \r\n" );
	mbedtls_net_free( &server_fd );
	mbedtls_ssl_free( &ssl );
	mbedtls_ssl_config_free( &conf );
	mbedtls_x509_crt_free( &cacert );

exit1:
	if(is_task) {
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
		printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif

		if(min_heap_size > 0)
			printf("\n\rMin available heap size = %d bytes during %s\n\r", min_heap_size, __FUNCTION__);

		vTaskDelete(NULL);
	}

	if(param != NULL)
		*((int *) param) = ret;
}
#endif

void start_ssl_client(void)
{
	is_task = 1;
	//strcpy(server_host, SERVER_HOST);

#if defined(CONFIG_MBEDTLS_VERSION3) && (CONFIG_MBEDTLS_VERSION3 == 1)
	if(xTaskCreate(ssl_client_TLS13, "ssl_client_TLS13", STACKSIZE, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate failed", __FUNCTION__);
#else
	if(xTaskCreate(ssl_client, "ssl_client", STACKSIZE, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate failed", __FUNCTION__);
#endif
}

void do_ssl_connect(void)
{
	int ret;
	static int success = 0;
	static int fail = 0;

	is_task = 0;
	strcpy(server_host, SERVER_HOST);
	ssl_client(&ret);

	if(ret != 0)
		printf("\n\r%s fail (success %d times, fail %d times)\n\r", __FUNCTION__, success, ++ fail);
	else
		printf("\n\r%s success (success %d times, fail %d times)\n\r", __FUNCTION__, ++ success, fail);
}

void cmd_ssl_client(int argc, char **argv)
{
	if(argc == 2) {
		strcpy(server_host, argv[1]);
	}
	else {
		printf("\n\rUsage: %s SSL_SERVER_HOST", argv[0]);
		return;
	}

	start_ssl_client();
}

#endif /* CONFIG_USE_POLARSSL */