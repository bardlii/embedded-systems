/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Bradley Jocelyn (bcj2124) & Aymen Norain (aan2161)
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>
#include "keystateToASCII.h"

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128
#define MAX_MESSAGE_LENGTH 64 /* Maximum length of a message line */
#define MAX_MESSAGES 20 /* Maximum number of messages to store */

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);

/* Useful global variables */
int separator_row; /* Row where line is drawn */
int display_area_rows; /* Number of rows available for messages */
int total_rows, total_cols; /* Total rows and columns on screen */

/* Message buffer */
char message_buffer[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
int message_count = 0;
pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Function prototypes */
void add_message(const char *message);
void display_messages();
void clear_display();

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  //* Clear the screen and draw a horizontal line across the bottom */
  /* of the screen.  The line is drawn with the '-' character. */
  fbclear();

  // * Determine the total number of rows and columns that can fit on the screen */
  total_rows = fb_total_rows();
  total_cols = fb_total_cols();

  /* Draw a horizontal line across the bottom of the screen to seperate inbound/outbound chats*/
  int separator_row = total_rows - 3;
  fb_horizontal_line(separator_row, '-');

  display_area_rows = separator_row - 1;

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('*', 23, col);
  }

  fbputs("Hello CSEE 4840 World", 4, 10);

  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }
    
  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  char userArrayInput[BUFFER_SIZE] = {0};
  int cursorHorizontalPosition = 0;
  int cursorVerticalPosition = separator_row + 1; /* Start below the separator line */

  /* Display text prompt for user */
  // fbputs("Enter text: ", cursorVerticalPosition, 0);
  
  /* Look for and handle keypresses */
  for (;;) {
    /* Read the keyboard input */
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, packet.keycode[0],
	      packet.keycode[1]);
      printf("%s\n", keystate);      
      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
	      break;
      }

      char *userTextInput = keystateToASCII(keystate);
      if (userTextInput[0] != '\0') { /* Ignore null character, user hasn't pressed anything. */

        if (userTextInput[0] == '\n') { /* Enter key pressed */
          /* Send message to server and input it into buffer*/
          write(sockfd, userArrayInput, strlen(userArrayInput));
          // add_message(userArrayInput); /* Might not be necessary if it's handled in network thread?*/
          
          /* Reset cursor position */
          cursorHorizontalPosition = 0;
          cursorVerticalPosition = separator_row + 1;
          
          /* Display prompt and set cursor after*/
          fbputs("Enter text: ", cursorVerticalPosition, 0); 
          cursorHorizontalPosition = strlen("Enter text: ");

          /* Clear the input buffer */
          memset(userArrayInput, 0, sizeof(userArrayInput));
          fbputchar(' ', cursorVerticalPosition, cursorHorizontalPosition); /* Clear character on screen */

        } else if (userTextInput[0] == '\b') { /* Backspace key pressed */
          if (cursorHorizontalPosition > 0) {
            cursorHorizontalPosition--;
            userArrayInput[cursorHorizontalPosition] = '\0'; /* Null terminate the string */
            fbputchar(' ', cursorVerticalPosition, cursorHorizontalPosition); /* Clear character on screen */
          }
  
        } else if ((userTextInput[0] == '<') && ((cursorHorizontalPosition > 0))) { /* Left arrow key pressed */
            cursorHorizontalPosition--;

        } else if (userTextInput[0] == '>') { /* Right arrow key pressed */
          if (cursorHorizontalPosition < BUFFER_SIZE - 1) { //MOVE INTO ONE CONDITIONAL FOR ONE LINE
            cursorHorizontalPosition++;
          }

        } else if (userTextInput[0] == '\0') { /* Ignore null character */
          continue;

        } else if (cursorHorizontalPosition >= BUFFER_SIZE - 1) { /* Ignore input if buffer is full */
          continue;

        } else {
          userArrayInput[cursorHorizontalPosition++] = userTextInput[0]; /* Add character to array */
          fbputchar(userTextInput[0], cursorVerticalPosition, cursorHorizontalPosition - 1); /* Display character on screen */
        }
      }
  

    }
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;

}

void add_message(const char *message) {
  pthread_mutex_lock(&message_mutex);
  
  /* Shift messages up to make room */
  if (message_count == MAX_MESSAGES) {
    for (int i = 0; i < message_count; i++) {
      strcpy(message_buffer[i], message_buffer[i + 1]);
    }
    message_count--;
  }

  /* Add new message to buffer */
  strncpy(message_buffer[message_count], message, MAX_MESSAGE_LENGTH - 1);
  message_buffer[message_count][MAX_MESSAGE_LENGTH - 1] = '\0';
  message_count++;

  display_messages();

  pthread_mutex_unlock(&message_mutex);
}

void display_messages(void) {
  clear_display();

  /* Calculate how many messages we can display */
  int displayable = message_count < display_area_rows ? message_count : display_area_rows;

  for (int i = 0; i < displayable; i++) {
    int msg_index = message_count - displayable + i;
    int row = separator_row - displayable + i;

    fbputs(message_buffer[msg_index], row, 0);
  }
}

void clear_display(void) {
  fbputs("hit clear display", 0, 0);
  for (int row = 0; row < separator_row; row++) {
    for (int col = 0; col < total_cols; col++) {
      fbputchar(' ', row, col);
    }
  }
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;

  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
    
    /* Add prefix for received messages */
    char display_msg[MAX_MESSAGE_LENGTH];
    snprintf(display_msg, MAX_MESSAGE_LENGTH, "Them: %s", recvBuf);

    /* Display message*/
    add_message(display_msg);
  }

  return NULL;

  /*
void draw_cursor(int row, int col) {
    unsigned char *pixel = framebuffer +
        (row * FONT_HEIGHT * 2 + fb_vinfo.yoffset) * fb_finfo.line_length +
        (col * FONT_WIDTH * 2 + fb_vinfo.xoffset) * BITS_PER_PIXEL / 8;

    for (int y = 0; y < FONT_HEIGHT * 2; y++) {
        for (int x = 0; x < FONT_WIDTH * 2; x++) {
            pixel[0] = 255; // Red
            pixel[1] = 255; // Green
            pixel[2] = 255; // Blue
            pixel[3] = 0;   // Alpha
            pixel += 4;     // Move to the next pixel
        }
        pixel += fb_finfo.line_length - (FONT_WIDTH * 2 * 4); // Move to the next row
    }
}

void erase_cursor(int row, int col, char c) {
    fbputchar(c, row, col); // Redraw the character at the cursor position
}
    */

}

