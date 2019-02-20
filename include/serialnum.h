#if !defined(_IGOLGI_SERIAL_NUM_H_)
#define _IGOLGI_SERIAL_NUM_H_

#if defined(__cplusplus)
extern "C" {
#endif

#define VERIFY_FAILED 0
#define VERIFY_OK_NEEDS_RECHECK 1
#define VERIFY_OK_NO_RECHECK 2

    int verify_file(char *filename, int channel, int is_initial);
    int get_serial_number(char *serial_num, int channel);
    int get_mac_address(const char *iface_name, unsigned char *mac_addr);

#if defined(__cplusplus)
}
#endif

#endif // _IGOLGI_SERIAL_NUM_H_
