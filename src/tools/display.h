/**
 * SH display
 *
 */

#ifndef  DISPLAY_H
#define  DISPLAY_H

/**
 * An opaque handle to the display.
 */
struct DISPLAY;
typedef struct DISPLAY DISPLAY;

/**
 * Open the display
 * \retval 0 Failure
 * \retval >0 Handle
 */
DISPLAY *display_open(void);

/**
 * Close the frame buffer
 * \param disp Handle returned from display_open
 */
void display_close(DISPLAY *disp);

/**
 * Get the width of the display in pixels
 * \param disp Handle returned from display_open
 */
int display_get_width(DISPLAY *disp);

/**
 * Get the height of the display in pixels
 * \param disp Handle returned from display_open
 */
int display_get_height(DISPLAY *disp);

/**
 * Get a pointer to the back buffer
 * \param disp Handle returned from display_open
 */
unsigned char *display_get_back_buff_virt(DISPLAY *disp);

/**
 * Get the physical address of the back buffer
 * \param disp Handle returned from display_open
 */
unsigned long display_get_back_buff_phys(DISPLAY *disp);

/**
 * Place the back buffer on the screen
 * \param disp Handle returned from display_open
 */
int display_flip(DISPLAY *disp);

#endif
