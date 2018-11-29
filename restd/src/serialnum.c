#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <syslog.h>

#include "libconfig.h"

#include "serialnum.h"
#include "config.h"

#if !defined(DISABLE_DEBUG_PRINTF)
#define SERIALNUM_DEBUG
#endif

#define DISABLE_V1
//#define IGOLGI_PRIVATE_KEY

#if defined(SERIALNUM_DEBUG)
#define __PRINTF(fmt, args...) { fprintf(stderr,fmt , ## args); fflush(stderr); }
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)
#else
#define DEBUG_PRINTF(fmt, args...)
#endif

#if defined(USE_DMALLOC)
#include "dmalloc.h"
#endif

#ifdef ENABLE_KEYLOK_SERIAL
#include "keylok.h"
int get_kls(unsigned short *kls);
#endif

#ifdef MP4CLIPPER
// Keys used by MP4 clipper only
#if !defined(DISABLE_V1)
#if defined(IGOLGI_PRIVATE_KEY)
static char *igolgi_privkey = "-----BEGIN RSA PRIVATE KEY-----\nMIICXAIBAAKBgQDCOKmJBpt9ib2Ujee03rwyxuv1YY81djS1X9b10xsIQ7czX7e/\nscDOLE5LuFpfCwSdxZPvEfOJySMq94vgdLPsq/9ASVQCwey9bsiC4ATDYmL926Dz\n2ny5s4cvf0N8Of32GWXnbnKCgjogmSULh1GfbazJf3RooHFZCYAmAhe/CQIDAQAB\nAoGAQiq5n0QP/wHJA24gzR7AsO/R/UPiXQ1LQatH+XGVGQiwxiDK4dS14cd4WRWS\nPCTtyq5ACsdr17odcArrrWk0zklq4/tpRFvhLzds+Xy540EPY5s99+vLn88A1MFO\nztDEk5sWIeoEWIswkYULqlt1dJhozx5nMnmgd9mfr+6Bm80CQQDo6qN//i+J6WZy\nxNAh++Dh1KczZfD7lO7SZd7LWilkquUJQn9FvyBG+ghdFSbPf7Mh8q6LmFT0c7qp\nAN6Y+95HAkEA1XhIAPzxmhMyMdhHF5FjEgFqG4xfI8xKTf3sM8XqU329FDNDplR8\nNQXoim39YT4sHeSafFuo5D5FBZbM+Z6QLwJARR/t6jyL60gjqYgTpSJVuXAdNznX\n7TJkNnkZSAy4IDI/yyG7F/4DHE10UfvHCuoBRd/6QV+yRuJZ0XJ6nbiSXQJAJHH9\nsRV+VjTSzAnF0XND+838BKoJkD4PrZMdoZU3tXtxLaK2+Q3RiufwVLoEmXaY552g\n9nxbsGQlOpgNdMyqhwJBAJnSFxfdlKOqLqXr50WULC3dt5Ptfw2TY5RaK/AneoOx\no+0yh4lU7zDrlxKuSq0gq/nKdihI3cv+CtZB0zgoqdE=\n-----END RSA PRIVATE KEY-----\n";
#endif
#endif
static char *igolgi_pubkey = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDCOKmJBpt9ib2Ujee03rwyxuv1\nYY81djS1X9b10xsIQ7czX7e/scDOLE5LuFpfCwSdxZPvEfOJySMq94vgdLPsq/9A\nSVQCwey9bsiC4ATDYmL926Dz2ny5s4cvf0N8Of32GWXnbnKCgjogmSULh1GfbazJ\nf3RooHFZCYAmAhe/CQIDAQAB\n-----END PUBLIC KEY-----";

#else
// Keys used by encoder, mux, demux, fileapp
#if !defined(DISABLE_V1)
#if defined(IGOLGI_PRIVATE_KEY)
static char *igolgi_privkey = "-----BEGIN RSA PRIVATE KEY-----\nMIICXQIBAAKBgQDcW7+CwttYAYkL0CciigZQ4WEU7j39n/Ji9IJwN2G5NPUkjS6E\n109d9xM9uheyru9/lq9MxbzpDk0SfbTp9Qcht7R8OqYKMJtdVB1MM/v30s4+oYMz\njM54pBMrpMhsXg/0l5OM9LXFY3WxhzO5/0jkflqBFJTBSjFSfXQbSjfYpQIDAQAB\nAoGAdfRiwlMd6LEBtCIbIMDzeo36UqLo6f+ZVuD9haYPmH1Bj+xG73L1mB3u3cbk\ncpBzwT9e3OCoK7StCu3hTq8LvkFrtxjzOnMj7VHI7VC9/pjbQmXvyZYBKjCLWi7X\nC0dmnYp3BFDdEm7jcp6jpPveBhuwIvAFsmYPwjvF6dBsrG0CQQDyhn73Afe7EMZv\nZGFMbcej5ylLkincqIEsZvYJ7P9QPmYnuC7iaeqk7JAJiyM39kA8SncRreKFKdjc\nZAXi1vDvAkEA6Jn3Ejh2Nj2xglglZUPzBloh20r3l5Cwj493iRYYoIxRsVyP673H\nfnkB1YgpfxBd6cqi5AHf8kABgpZ7mBenqwJAIGfyTSUZKgjKyxWZnrHIjFEWBoAI\nUfC+GeXEGH1vfBRqaAJHWX+Xl+P4Nx49XXvtB2FX2afnba2yyXggBTh9RwJBAK3O\nVqOj0xlG0jxut0rLEm7drMzbYNU9heFQN+cUvsRA9c5NNzHVdptXunofq7pJtZM8\nGqm7iObQ0xAMeEA29M8CQQCvJRn2xDONFrtrjI/2SqsOGspFICGtd/XDQo2m4t2d\neaETCuEB6PC7Aw8BHmF3ZMrW8BIPHJbGlA21nhtHK65e\n-----END RSA PRIVATE KEY-----\n";
#endif
#endif

static char *igolgi_pubkey = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDcW7+CwttYAYkL0CciigZQ4WEU\n7j39n/Ji9IJwN2G5NPUkjS6E109d9xM9uheyru9/lq9MxbzpDk0SfbTp9Qcht7R8\nOqYKMJtdVB1MM/v30s4+oYMzjM54pBMrpMhsXg/0l5OM9LXFY3WxhzO5/0jkflqB\nFJTBSjFSfXQbSjfYpQIDAQAB\n-----END PUBLIC KEY-----";

#endif

#define SIGNATURE_SIZE           128
#define MESSAGE_SIZE             1024
#define CURRENT_FORMAT_VERSION   1
#define VERIFY_FAILURE_THRESHOLD 5

static int verify_initialized = 0;
static int verify_failures = 0;
static char checksum_data[MESSAGE_SIZE];

int get_serial_number(char *serial_num, int channel);
int get_mac_address(const char *iface_name, unsigned char *mac_addr);
static int verify_file_v0(char *filename, int channel);

int finish_checksum(char* checksum) {
#ifdef ENABLE_KEYLOK_SERIAL
    unsigned short kls = 0;
    int num_kls_checks = 0;
    int kls_success = 0;
#endif
    // For some security mechanisms such as USB key, we need to redo the hardware check
    // every time the license is checked versus trusting cached data.
    strcpy(checksum, checksum_data);    
#ifdef ENABLE_KEYLOK_SERIAL     
    // get_kls briefly locks key so recheck in loop before failing in 
    // case a different process has the lock.
    while (num_kls_checks < 20) {
        if (get_kls(&kls) != -1) {
            kls_success = 1;
            break;
        }
        ++num_kls_checks;
        usleep(5000);
    }
    if (!kls_success) {
        fprintf(stderr,"ERROR: Please check that dongle is connected.\n");
        return 0;    
    }
    sprintf(checksum+strlen(checksum),"system6: %04X\n", kls);
#endif            
    return 1;
}

static size_t flen(FILE* f)
{
    int curpos, len;
    curpos = ftell(f);
    fseek(f, 0, SEEK_END);
    len = ftell(f); 
    fseek(f, curpos, SEEK_SET);
    return len;
}

int verify_init(int channel)
{
    int retval;
    char serial_number[80];
    unsigned char mac_addr[6];
    char *message = NULL;
    int i;
    int xval;

    if (verify_initialized)
        return -1;
        
    // Build unique signature for this machine            
    memset(checksum_data, 0, sizeof(checksum_data));
    message = &checksum_data[0];

    retval = get_serial_number((char *)&serial_number[0], channel);
    if (retval < 0) {
        return -1;   
    }
    retval = get_mac_address(get_config()->interface_name, (unsigned char *)&mac_addr[0]);
    if (retval < 0) {
        return -1;
    }
    for (i = 0; i < 6; i++) {    
        xval = (i + 72) & 0xff;
        mac_addr[i] = mac_addr[i] ^ xval;
    }
    
    // this is the message that was originally signed - we're just cross-checking it.
    sprintf(message,"system1: %s\nsystem2: %d.%d.%d.%d\nsystem3: %d.%d.%d.%d\n", 
            serial_number,
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
            mac_addr[4], mac_addr[5], 0xfa, 0xce);
            
    //DEBUG_PRINTF("[%d] %s", channel, message);
    //fprintf(stderr, "[%d]%s\n", strlen(message), message);

    verify_initialized = 1;       
    return 1;
}

int verify_file(char *filename, int channel, int is_initial)
{
    int verify_result = VERIFY_FAILED;

    verify_result = verify_init(channel);    
    
    if (VERIFY_FAILED != verify_result) {
#if !defined(DISABLE_V1)
        verify_result = verify_file_v1(filename, channel);
#else
        verify_result = verify_file_v0(filename, channel);
#endif
    }
    
    
    if (VERIFY_FAILED != verify_result)
    {
        DEBUG_PRINTF("Verification succeeded (%d).\n", verify_result);    
        verify_failures = 0;
        return verify_result;
    }
    else
    {
        if (is_initial) {
            DEBUG_PRINTF("Verification failed: %d\n", verify_result);
            return VERIFY_FAILED;
        }

        // Give some leeway just in case something external to our app interferes with license check
        ++verify_failures;
        if (verify_failures >= VERIFY_FAILURE_THRESHOLD)
        {
            DEBUG_PRINTF("Verification failures exceeded threshold (%d).\n", verify_failures);
            return VERIFY_FAILED;
        }
        else
        {
            DEBUG_PRINTF("Verification failed but less than threshold (%d < %d) so will recheck).\n", verify_failures, VERIFY_FAILURE_THRESHOLD);
            return VERIFY_OK_NEEDS_RECHECK;
        }
    }    
}

#if !defined(DISABLE_V1)
int verify_file_v1(char *filename, int channel)
{
    char full_checksum[MESSAGE_SIZE];    
    uint8_t signature_data[SIGNATURE_SIZE]; 
    int retval;
    unsigned char *signature = NULL;
    int verified = 0;
#if defined(IGOLGI_PRIVATE_KEY)
    RSA *private_key = NULL;
    BIO *priv_bio = NULL;
#endif
    RSA *public_key = NULL;
    BIO *pub_bio = NULL;
    unsigned int slen;
    int data_read = 0;
    FILE *key_file = NULL;
    size_t enc_size, key_data_size;
    unsigned char *key_block = NULL, *key_data = NULL;
    int enc_block_size, key_block_size, enc_index, key_index, dec_count;
    int try_v0 = 0;
    int success = 0;
    int sig_index, n_sigs;
    config_t licparams;
    time_t exptime, curtime;    
    config_setting_t *setting = NULL, *sig_cfg_array = NULL;
    int temp_int;
    int verify_result = VERIFY_FAILED;
    
    if (!verify_initialized)
        goto cleanup;
               
    if (!finish_checksum(full_checksum)) {
        goto cleanup;
    }        
        
    // Init keys

#if defined(IGOLGI_PRIVATE_KEY)    
    priv_bio = BIO_new_mem_buf(igolgi_privkey, -1);
    if (priv_bio == NULL) {
        goto cleanup;
    }

    private_key = PEM_read_bio_RSAPrivateKey(priv_bio, NULL, NULL, NULL);
    if (private_key == NULL) {        
        goto cleanup;
    }
#endif

    pub_bio = BIO_new_mem_buf(igolgi_pubkey, -1);
    if (pub_bio == NULL) {
        goto cleanup;
    }
    
    public_key = PEM_read_bio_RSA_PUBKEY(pub_bio, NULL, NULL, NULL);
    if (public_key == NULL) {
        goto cleanup;
    }
    
    // Check key file existence    
    key_file = fopen(filename, "rb");
    if (!key_file)
        goto cleanup;
    
#if defined(IGOLGI_PRIVATE_KEY)
    // Build signature
    signature = (unsigned char*)malloc(RSA_size(private_key));
    if (RSA_sign(NID_md5, (unsigned char*)full_checksum, strlen(full_checksum), signature, &slen, private_key) != 1) {
        goto cleanup;
    }
#endif

    // Load key file and decrypt
    
    enc_size = flen(key_file);
    key_data = malloc(enc_size);
    if (enc_size != fread(key_data, 1, enc_size, key_file))
    {
        goto cleanup;
    }    
    fclose(key_file);      
    key_file = NULL;    
    
    enc_block_size = RSA_size(private_key); 

    key_block_size = enc_block_size - 11; // 11 is for RSA_PKCS1_PADDING;
    key_block = malloc(key_block_size);
    
    enc_index = 0;
    key_index = 0;    
    
    while (enc_index < enc_size)
    {             
        dec_count = RSA_private_decrypt(enc_block_size, key_data+enc_index, key_block, private_key, RSA_PKCS1_PADDING);
        if (-1 == dec_count)
        {
            // If decryption fails, it means wasn't RSA encrypted so try the old algorithm
            try_v0 = 1;
            goto cleanup;
        }            
        
        memcpy(key_data+key_index, key_block, dec_count);
        
        key_index += dec_count;
        enc_index += enc_block_size;        
    }
    
    key_data_size = key_index;
    key_index = 0;
    key_data[key_data_size] = '\0';
    
    // Load key file 
    config_init(&licparams);
    retval = config_read_string(&licparams, (char*)key_data);
    if (retval == 0)
    {
        try_v0 = 1;
        goto cleanup;
    }
    
    // At this point, encryption and cfg parsing have succeeded so no point in
    // falling back to try_v0 after this.

    // Initial verification checks of key file 
    
    if (CONFIG_FALSE == config_lookup_int(&licparams, "license.version", &temp_int))
        goto cleanup;
        
    if (temp_int != CURRENT_FORMAT_VERSION)
    {
        goto cleanup;
    }
        
    // Loop through signatures in key file to see if this machine matches
    
    sig_cfg_array = config_lookup(&licparams, "license.signatures");
    if (!sig_cfg_array)
        goto cleanup;
        
    n_sigs = config_setting_length(sig_cfg_array);
    if (n_sigs <= 0)
    {
        goto cleanup;
    }
    
    verified = 0;
    for (sig_index= 0; sig_index < n_sigs; ++sig_index)
    {
        BIO *b64, *bmem, *decoder;
        char b64sig[1024];    
        uint8_t signature_data[SIGNATURE_SIZE]; 
        int readlen;
         
        strcpy(b64sig, config_setting_get_string_elem(sig_cfg_array, sig_index));
        
        b64 = BIO_new(BIO_f_base64());         
        bmem = BIO_new_mem_buf(b64sig, strlen(b64sig));
        decoder = BIO_push(b64, bmem);
         
        BIO_flush(decoder);
         
        readlen = BIO_read(decoder, signature_data, SIGNATURE_SIZE);
        if (readlen != SIGNATURE_SIZE)
        {
            goto cleanup;
        }
         
        BIO_free_all(decoder);
        
        verified = RSA_verify(
                        NID_md5, 
                        (unsigned char*)full_checksum, 
                        strlen(full_checksum), 
                        signature_data, 
                        slen, 
                        public_key);                        
        if (verified) 
            break;
    }
    
    key_index += SIGNATURE_SIZE * n_sigs;
    
    if (!verified)
    {
        goto cleanup;
    }    
    
    if (key_index >= key_data_size)
    {
        goto cleanup;
    }
    
    // Parse and apply params block
    
    config_lookup_int(&licparams, "license.expiration", &temp_int);
    if (temp_int)
    {
        exptime = (time_t)temp_int;
            
        curtime = time(NULL);    
        if (curtime >= exptime)
        {
            fprintf(stderr, "LICENSE HAS EXPIRED\n");
            syslog(LOG_ERR, "LICENSE HAS EXPIRED\n");
            goto cleanup;        
        }
        
        // Give 24 hour warning of license expiration
        if (exptime - curtime < 24*3600)
        {
            fprintf(stderr, "LICENSE EXPIRING SOON - UTC %s", asctime(gmtime(&exptime)));
            syslog(LOG_WARNING, "LICENSE EXPIRING SOON - UTC %s", asctime(gmtime(&exptime)));
        }
        else
        {
            DEBUG_PRINTF("License expires UTC %s", asctime(gmtime(&exptime)));        
        }        
        
        verify_result = VERIFY_OK_NEEDS_RECHECK;
    }
    else
    {
        DEBUG_PRINTF("Non-expiring license\n");
        verify_result = VERIFY_OK_NO_RECHECK;    
    }
        
cleanup:
    if (key_file)
        fclose(key_file);
        
    if (key_block)
        free(key_block);
        
    if (key_data)
        free(key_data);

    if (private_key)
        RSA_free(private_key);
        
    if (priv_bio)
        BIO_free(priv_bio);
    
    if (public_key)
        RSA_free(public_key);
        
    if (pub_bio)
        BIO_free(pub_bio);
        
    if (VERIFY_FAILED != verify_result && !try_v0) 
    {
        DEBUG_PRINTF("verify_file_v1 returning %d\n", verify_result);   
        return verify_result;
    } 
    else {    
        verify_result = verify_file_v0(filename, channel);
        DEBUG_PRINTF("verify_file_v0 fallback returning %d\n", verify_result);
        return verify_result;        
    }
}
#endif

static int verify_file_v0(char *filename, int channel)
{
    FILE *check_file = NULL;
    char full_checksum[MESSAGE_SIZE];    
    uint8_t signature_data[SIGNATURE_SIZE]; 
    uint8_t *psignature_data;
    int retval;
    unsigned char *signature = NULL;
    int verified = 0;
#if defined(IGOLGI_PRIVATE_KEY)
    RSA *private_key = NULL;
    BIO *priv_bio = NULL;
#endif
    BIO *pub_bio = NULL;
    RSA *public_key = NULL;
    unsigned int slen;
    int data_read = 0;
    int verify_result = VERIFY_FAILED;

    if (!verify_initialized) {
        goto cleanup;
    }
        
    if (!finish_checksum(full_checksum)) {
        goto cleanup;
    }

    check_file = fopen(filename,"r");
    if (!check_file) {        
        goto cleanup;
    }

#if defined(IGOLGI_PRIVATE_KEY)
    priv_bio = BIO_new_mem_buf(igolgi_privkey, -1);
    if (priv_bio == NULL) {
        goto cleanup;
    }

    private_key = PEM_read_bio_RSAPrivateKey(priv_bio, NULL, NULL, NULL);
    if (private_key == NULL) {        
        goto cleanup;
    }

    signature = (unsigned char*)malloc(RSA_size(private_key));
    if (RSA_sign(NID_md5, (unsigned char*)full_checksum, strlen(full_checksum), signature, &slen, private_key) != 1) {
        goto cleanup;
    }
#endif

    pub_bio = BIO_new_mem_buf(igolgi_pubkey, -1);
    if (pub_bio == NULL) {
        goto cleanup;
    }
    
    public_key = PEM_read_bio_RSA_PUBKEY(pub_bio, NULL, NULL, NULL);
    if (public_key == NULL) {
        goto cleanup;
    }
 
    while (!feof(check_file)) {      
        memset(signature_data,0,sizeof(signature_data));
        data_read = fread(signature_data, SIGNATURE_SIZE, 1, check_file);
        if (data_read) {
            slen = 128;
            verified = RSA_verify(NID_md5,
                                  (unsigned char*)full_checksum,
                                  strlen(full_checksum),
                                  signature_data,
                                  slen,
                                  public_key);
            if (verified) {             
                verify_result = VERIFY_OK_NO_RECHECK;
                goto cleanup;
            }
        } else {
            goto cleanup;
        }          
    }

cleanup:
    if (check_file)
        fclose(check_file);
#if defined(IGOLGI_PRIVATE_KEY)
    if (signature)
        free(signature);
    if (private_key)
        RSA_free(private_key);        
    if (priv_bio)
        BIO_free(priv_bio);    
#endif
    if (public_key)
        RSA_free(public_key);        
    if (pub_bio)
        BIO_free(pub_bio);
    
    return verify_result;  
}

int get_mac_address(const char *iface_name, unsigned char *mac_addr)
{
    struct ifreq if_list[10];
    struct ifreq if_req;
    struct ifconf if_conf;
    int num_interfaces;
    int sock;
    int ret;
    int i;
    int ok = 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        return -1;
    }

    if_conf.ifc_buf = (char *) if_list;
    if_conf.ifc_len = sizeof(if_list);

    ret = ioctl(sock, SIOCGIFCONF, &if_conf);
    if (ret == -1)
    {
        close(sock);
        return -1;
    }

    num_interfaces = if_conf.ifc_len / sizeof(struct ifreq);

    for (i = 0; i < num_interfaces; i++)
    {
        if (!strcmp(if_list[i].ifr_name, iface_name))
        {
            strcpy(if_req.ifr_name, if_list[i].ifr_name);

            if (ioctl(sock, SIOCGIFHWADDR, &if_req) == 0)
            {
                memcpy(mac_addr, if_req.ifr_hwaddr.sa_data, 6);
                close(sock);
                return 0;
            }
        }
    }

    close(sock);
    return -1;
}

int get_serial_number(char *serial_num, int channel)
{
    char serial[80];
    char *message;
    char *serialout;
    FILE *tempfile = NULL;
    int slen, retval = 0;
    char tempfilename[256];
    char callcommand[256];
    
    memset(tempfilename, 0, sizeof(tempfilename));
    sprintf(tempfilename,"/var/tmp/igolgi%d.txt", getpid());

    memset(callcommand, 0, sizeof(callcommand));
    sprintf(callcommand,"/usr/sbin/dmidecode --type 2 | grep -i serial > %s", tempfilename);
    retval = system(callcommand);
    tempfile = fopen(tempfilename,"r");
    if (!tempfile) {      
        unlink(tempfilename);
        return -1;
    }
    
    message = fgets(serial, 79, tempfile);  
    fclose(tempfile);

    serialout = strstr(serial,"Serial Number: ");
    if (serialout) {
        serialout += strlen("Serial Number: ");
    } else {
        unlink(tempfilename); 

        sprintf(callcommand,"/usr/sbin/dmidecode --type 1 | grep -i serial > %s", tempfilename);
        retval = system(callcommand);
    
        tempfile = fopen(tempfilename,"r");
        if (tempfile) {
            memset(serial, 0, sizeof(serial));
            message = fgets(serial, 79, tempfile);
            fclose(tempfile);
            serialout = strstr(serial,"Serial Number: ");
            if (serialout) {
                serialout += strlen("Serial Number: ");
            } else { 
                unlink(tempfilename);
                return -1;
            }
        }
    }

    unlink(tempfilename);

    message = serialout;

    slen = strlen(message);
    message[slen-1] = '\0';

    if (serial_num) {      
        sprintf(serial_num, "%s", message);
    }

    return 0;
}

#ifdef ENABLE_KEYLOK_SERIAL
void KTASK(unsigned CommandCode, unsigned Argument2, 
           unsigned Argument3, unsigned Argument4, unsigned short* ReturnVal1, unsigned short* ReturnVal2)
{
    unsigned ReturnValue;
    ReturnValue = KFUNC(CommandCode, Argument2, Argument3, Argument4);
    *ReturnVal1 = (unsigned) (ReturnValue & 0XFFFF);
    *ReturnVal2 = (unsigned) (ReturnValue >> 16);
}

unsigned RotateLeft(unsigned Target, int Counts)
{
  int i;
  static unsigned LocalTarget, HighBit;

  LocalTarget = Target;
  for (i=0; i<Counts; i++)
  {
   HighBit = LocalTarget & 0X8000;
   LocalTarget = (LocalTarget << 1) + (HighBit >> 15);
  }
  LocalTarget = LocalTarget & 0XFFFF; /* For 32 bit integers */
  return (LocalTarget);
}

int get_kls(unsigned short *kls) {
    unsigned long ReturnValue;
    unsigned short ReturnVal1, ReturnVal2;
    *kls = 0;
    KTASK((unsigned)(KLCHECK), ValidateCode1, ValidateCode2, ValidateCode3, &ReturnVal1, &ReturnVal2);
    KTASK(RotateLeft(ReturnVal1, ReturnVal2 & 7) ^ ReadCode3 ^ ReturnVal2,
          RotateLeft(ReturnVal2, ReturnVal1 & 15),
          ReturnVal1 ^ ReturnVal2, 0, &ReturnVal1, &ReturnVal2);    
          
    if ((ReturnVal1 == ClientIDCode1) && (ReturnVal2 == ClientIDCode2)) {
        KTASK(READAUTH, ReadCode1, ReadCode2, ReadCode3, &ReturnVal1, &ReturnVal2);
        KTASK(GETSN, rand(), rand(), rand(), &ReturnVal1, &ReturnVal2);        
        *kls = ReturnVal1;
        KTASK((unsigned)-1, rand(), rand(), rand(), &ReturnVal1, &ReturnVal2);  
        return 0;
    }
    KTASK((unsigned)-1, rand(), rand(), rand(), &ReturnVal1, &ReturnVal2);  
    return -1;
}
#endif
