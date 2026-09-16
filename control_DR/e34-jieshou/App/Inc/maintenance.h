#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Non-blocking USB CDC loopback.
 *
 * Must be called after ReceiverControl_Poll(). This stage only proves the
 * virtual COM port can receive and send; no maintenance protocol yet.
 * USB not connected or TX busy is skipped; the 300 ms failsafe is untouched.
 */
void Maintenance_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* MAINTENANCE_H */
