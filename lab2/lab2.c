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
pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Function prototypes */
void add_message(const char message[2][MAX_MESSAGE_LENGTH]);
void clear_display();
void clear_input();

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

  /* Clear the screen and draw a horizontal line across the bottom */
  /* of the screen.  The line is drawn with the '-' character. */
  fbclear();

  /* Determine the total number of rows and columns that can fit on the screen */
  total_rows = fb_total_rows();
  total_cols = fb_total_cols();

  /* Draw a horizontal line across the bottom of the screen to seperate inbound/outbound chats*/
  separator_row = total_rows - 4;
  fb_horizontal_line(separator_row, '-');

  display_area_rows = separator_row - 1;

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('*', 23, col);
  }

  // fbputs("Hello CSEE 4840 World", 4, 10);

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

  char userArrayInput[2][MAX_MESSAGE_LENGTH];
  /* Clear this array */
  for (int rows = 0; rows < 2; rows++) {
    for (int cols = 0; cols < MAX_MESSAGE_LENGTH; cols++) {
      userArrayInput[rows][cols] = '\0';
    }
  }

  int cursorHorizontalPosition = 0;
  int cursorVerticalPosition = separator_row + 1; /* Start below the separator line */
  fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition); /* Place initial cursor */  
  
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
          /* Combine both rows to send through socket */
          char combinedMessage[BUFFER_SIZE]; // Extra space for newline or null terminator
          snprintf(combinedMessage, sizeof(combinedMessage), "%s%s", userArrayInput[0], userArrayInput[1]);
          
          /* Send message to server*/
          write(sockfd, combinedMessage, strlen(combinedMessage));
          
          /* Clear the input buffer and combined message buffer*/
          memset(userArrayInput[0], 0, MAX_MESSAGE_LENGTH);
          memset(userArrayInput[1], 0, MAX_MESSAGE_LENGTH);
          combinedMessage[0] = '\0';
          
          /* Clear input area */
          clear_input();

          /* Reset cursor position */
          cursorHorizontalPosition = 0;
          cursorVerticalPosition = separator_row + 1;
          fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition); /* Place cursor */

        } else if (userTextInput[0] == '\b') { /* Backspace key pressed */
          if (cursorHorizontalPosition > 0) {
            cursorHorizontalPosition--;
            // Shift any characters to the left
            for (int i = cursorHorizontalPosition; i < strlen(userArrayInput[0]); i++) {
              userArrayInput[cursorVerticalPosition - (separator_row + 1)][i] = userArrayInput[cursorVerticalPosition - (separator_row + 1)][i + 1];
            }
            fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition); 
          } else {
            continue;
          }
          
        } else if (packet.keycode[0] == 0x50) { /* Left arrow key pressed */
            int rowIndex = cursorVerticalPosition - (separator_row + 1);
            char charAfter;

            // Null character at first row, first line
            /* 
            if ((rowIndex == 0) && (userArrayInput[rowIndex][cursorHorizontalPosition] == '\0')) {
              fbputchar(' ', cursorVerticalPosition, cursorHorizontalPosition); // Clear the character at the cursor position
              continue;
            // Start of first row with character
            } else 
            */

           // only null character at the end of the second row, shouold go left, all others should not do anything

            if (rowIndex == 0 && cursorHorizontalPosition == 0) {
              // Do nothing, already at the start of the first row
              continue;
            } else if (rowIndex > 0 && cursorHorizontalPosition == 0) {
              // Move to the last character of the first row
              cursorVerticalPosition--; // Move to the first row
              charAfter = userArrayInput[1][0];

            // MINUS 2 OR MINUS 1 FOR MAX_MESSAGE_LENGTH
              cursorHorizontalPosition = MAX_MESSAGE_LENGTH - 1; // Move to the last character of the first row, but not the null terminator
              fbputchar(charAfter, 1, 0); /* Restore the previous character on the second row, first column */
              fbputchar("_", 0, cursorHorizontalPosition+1); /* Place cursor on the 63rd index of display, but dont actually change the value of it in user array*/
              printf("Left arrow pressed, shift row, cursor at (%d, %d)\n", cursorHorizontalPosition, cursorVerticalPosition);
              printf("Character after: %c\n", charAfter);
              printf("Current character to edit: %c\n", userArrayInput[rowIndex][cursorHorizontalPosition]);
              printf("Row index: %d\n", rowIndex);
              // Move left within the same row
            } else if (cursorHorizontalPosition > 0) {
                // Move left within the first row
                charAfter = userArrayInput[rowIndex][cursorHorizontalPosition];
                fbputchar(charAfter, cursorVerticalPosition, cursorHorizontalPosition); /* Restore the character to the right of cursor */
                cursorHorizontalPosition--; /* Move cursor left */
                fbputchar("_", cursorVerticalPosition, cursorHorizontalPosition); /* Place cursor */

                printf("Left arrow pressed, cursor at (%d, %d)\n", cursorHorizontalPosition, cursorVerticalPosition);
                printf("Character after: %c\n", charAfter);
                printf("Current character to edit: %c\n", userArrayInput[rowIndex][cursorHorizontalPosition]);
                printf("Row index: %d\n", rowIndex);
            } 
            
        
        } else if (userTextInput[0] == '>') { /* Right arrow key pressed */
            // Check that we are not at the last column and that the next character is present.
            if (cursorHorizontalPosition < total_cols - 1 && userArrayInput[cursorVerticalPosition - (separator_row + 1)][cursorHorizontalPosition + 1] != '\0') {
            // Restore the original character at the current cursor position, in case the cursor indicator was drawn
            fbputchar(userArrayInput[cursorVerticalPosition - (separator_row + 1)][cursorHorizontalPosition], cursorVerticalPosition, cursorHorizontalPosition);
            // Move the cursor one position to the right
            cursorHorizontalPosition++;
            // Draw the cursor indicator at the new position
            fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition);
            } else {
            continue; // Ignore if at the end of the line
            }

        } else if ((cursorHorizontalPosition >= MAX_MESSAGE_LENGTH - 1) && (cursorVerticalPosition == separator_row + 2)) { /* Ignore input if buffer is full */
          continue;

        } else { /* ASCII character pressed */
          int rowIndex = cursorVerticalPosition - (separator_row + 1);
          if (rowIndex >= 0 && rowIndex < 2 && cursorHorizontalPosition < total_cols) { /* Stay within input display bounds */
            /* Add character to array */
            userArrayInput[rowIndex][cursorHorizontalPosition] = userTextInput[0];
            
            /* Ensure null-termination */
            userArrayInput[0][MAX_MESSAGE_LENGTH - 1] = '\0';
            userArrayInput[1][MAX_MESSAGE_LENGTH - 1] = '\0';
          }

          /* Display character on screen */
          fbputchar(userTextInput[0], cursorVerticalPosition, cursorHorizontalPosition);
          cursorHorizontalPosition++;

          /* Ensure cursor is displayed after the character */
          if (cursorHorizontalPosition < total_cols - 1) {
            fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition);
          }

          /* Text wrapping logic */
          if (cursorHorizontalPosition >= total_cols - 1) { /* Check if the current row is full */
            if (cursorVerticalPosition < separator_row + 2) { /* Ensure we don't exceed the input area */
              cursorHorizontalPosition = 0; /* Reset column position */
              cursorVerticalPosition++; /* Move to the next row */
              fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition); /* Place cursor */
            }
          }
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

void add_message(const char message[2][MAX_MESSAGE_LENGTH]) {
  pthread_mutex_lock(&message_mutex);

  /* Shift all messages up by two positions to make room for new messages */
  for (int i = 0; i < MAX_MESSAGES - 2; i++) {
    strcpy(message_buffer[i], message_buffer[i + 2]);
  }

  /* Add new messages to the buffer */
  strncpy(message_buffer[MAX_MESSAGES - 2], message[0], MAX_MESSAGE_LENGTH - 1);
  message_buffer[MAX_MESSAGES - 2][MAX_MESSAGE_LENGTH - 1] = '\0';

  strncpy(message_buffer[MAX_MESSAGES - 1], message[1], MAX_MESSAGE_LENGTH - 1);
  message_buffer[MAX_MESSAGES - 1][MAX_MESSAGE_LENGTH - 1] = '\0';

  /* Clear the display area */
  clear_display();

  /* Display all messages with proper wrapping */
  for (int i = 0; i < MAX_MESSAGES; i++) {
    int row = separator_row - (MAX_MESSAGES - i);
    if (row < 1 || row >= separator_row) continue;

    int col = 0;
    for (int j = 0; j < strlen(message_buffer[i]); j++) {
      if (col >= total_cols) { /* Wrap to next line if needed */
        row++;
        col = 0;
        if (row >= separator_row) break;
      }
      fbputchar(message_buffer[i][j], row, col++);
    }
  }

  pthread_mutex_unlock(&message_mutex);
}


void clear_display(void) {
  for (int row = 1; row < separator_row; row++) {
    for (int col = 0; col < total_cols; col++) {
      fbputchar(' ', row, col);
    }
  }
}

void clear_input(void) {
  for (int col = 0; col < total_cols; col++) {
    fbputchar(' ', separator_row + 1, col);
    fbputchar(' ', separator_row + 2, col);
  }
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;

  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0) {
    recvBuf[n] = '\0';
    printf("Received: %s\n", recvBuf);

    if (n == BUFFER_SIZE - 1) { /* Check if the message exactly filled the buffer */
      /* Discard any remaining data in this message */
      char discardBuf[BUFFER_SIZE];
      int discard_n;
      
      /* Use MSG_DONTWAIT to read non-blockingly until buffer is cleared */
      while ((discard_n = recv(sockfd, discardBuf, BUFFER_SIZE - 1, MSG_DONTWAIT)) > 0) {
        printf("Discarded %d additional bytes\n", discard_n);
      }
    }

    /* Prepare message for display */
    char display_msg[2][MAX_MESSAGE_LENGTH];
    
    /* Clear display message buffer */
    memset(display_msg[0], 0, MAX_MESSAGE_LENGTH);
    memset(display_msg[1], 0, MAX_MESSAGE_LENGTH);

    /* Split received message into two display lines if needed */
    if (strlen(recvBuf) <= MAX_MESSAGE_LENGTH - 1) {
      /* Short message fits in first line */
      strncpy(display_msg[0], recvBuf, MAX_MESSAGE_LENGTH - 1);
      display_msg[0][MAX_MESSAGE_LENGTH - 1] = '\0';
      /* Second line remains empty */
    } else {
      /* Message needs to be split */
      strncpy(display_msg[0], recvBuf, MAX_MESSAGE_LENGTH - 1);
      display_msg[0][MAX_MESSAGE_LENGTH - 1] = '\0';
      
      strncpy(display_msg[1], recvBuf + MAX_MESSAGE_LENGTH - 1, MAX_MESSAGE_LENGTH - 1);
      display_msg[1][MAX_MESSAGE_LENGTH - 1] = '\0';
    }

    /* Display message */
    add_message(display_msg);
  }
  
  return NULL;
}
