#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Non-blocking USB CDC host stream.
 *
 * Must be called after ReceiverControl_Poll(). Pushes a 24-byte CONTROL
 * snapshot every 20 ms. Host bytes are drained and ignored in v1.
 * USB not connected or TX busy is skipped; the 300 ms failsafe is untouched.
 */
void Maintenance_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* MAINTENANCE_H */
