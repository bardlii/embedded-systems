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
// void display_messages();
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
          /* Add message to display */
          // char display_msg[MAX_MESSAGE_LENGTH];
          // snprintf(display_msg, MAX_MESSAGE_LENGTH, "You: %s", userArrayInput[0]);
          // add_message(userArrayInput);

          /* Combine both rows to send through socket */
          char combinedMessage[MAX_MESSAGE_LENGTH * 2 + 2]; // Extra space for newline or null terminator
          snprintf(combinedMessage, sizeof(combinedMessage), "%s%s", userArrayInput[0], userArrayInput[1]);
          
          /* Send message to server*/
          write(sockfd, combinedMessage, strlen(combinedMessage));
          
          /* Clear the input buffer */
          memset(userArrayInput, 0, sizeof(userArrayInput));

          /* Clear userArrayInput buffer */
          for (int rows = 0; rows < 2; rows++) {
            for (int cols = 0; cols < MAX_MESSAGE_LENGTH; cols++) {
              userArrayInput[rows][cols] = '\0';
            }
          }
          
          /* Clear input area */
          clear_input();

          /* Reset cursor position */
          cursorHorizontalPosition = 0;
          cursorVerticalPosition = separator_row + 1;
          fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition);

        } else if (userTextInput[0] == '\b') { /* Backspace key pressed */
          if (cursorHorizontalPosition > 0) {
            cursorHorizontalPosition--;
            // Shift any characters to the left
            for (int i = cursorHorizontalPosition; i < strlen(userArrayInput[0]); i++) {
              userArrayInput[cursorVerticalPosition - (separator_row + 1)][i] = userArrayInput[cursorVerticalPosition - (separator_row + 1)][i + 1];
            }
            fbputchar(' ', cursorVerticalPosition, cursorHorizontalPosition);
            fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition); 
          } else {
            continue;
          }
          
        } else if (userTextInput[0] == '<') { /* Left arrow key pressed */
          if (cursorHorizontalPosition > 0) { 
            fbputchar(userArrayInput[cursorVerticalPosition - (separator_row + 1)][cursorHorizontalPosition], cursorVerticalPosition, cursorHorizontalPosition); //Restore previous characters
            cursorHorizontalPosition--;
            fbputchar(' ', cursorVerticalPosition, cursorHorizontalPosition);
            fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition); /* Place cursor */
          } else {
            continue;
          }

        } else if (userTextInput[0] == '>') { /* Right arrow key pressed */
          // Check that we are not at the last column and that the next character is present.
          if (cursorHorizontalPosition < total_cols - 1 && userArrayInput[inputRow][cursorHorizontalPosition + 1] != '\0') {
          // Restore the original character at the current cursor position, in case the cursor indicator was drawn
          fbputchar(userArrayInput[cursorVerticalPosition - (separator_row + 1)][cursorHorizontalPosition], cursorVerticalPosition, cursorHorizontalPosition);
          // Move the cursor one position to the right
          cursorHorizontalPosition++;
          // Draw the cursor indicator at the new position
          fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition);
          } else {
            continue; // Ignore if at the end of the line
          }

        } else if (userTextInput[0] == '\0') { /* Ignore null character */ //DELETE, UNNECESSARY?
          continue;

        } else if ((cursorHorizontalPosition >= MAX_MESSAGE_LENGTH - 1) && (cursorVerticalPosition == separator_row + 2)) { /* Ignore input if buffer is full */
          continue;

        } else {
          // /* Add character to array */
          // userArrayInput[cursorVerticalPosition - (separator_row + 1)][cursorHorizontalPosition] = userTextInput[0];
          
          // /* Display character on screen */
          // fbputchar(userTextInput[0], cursorVerticalPosition, cursorHorizontalPosition);
          // cursorHorizontalPosition++;

          // /* Ensure cursor is displayed after the character */
          // fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition);

          // /* Text wrapping logic */
          // if (cursorHorizontalPosition >= total_cols) { /* Check if the current row is full */
          //   if (cursorVerticalPosition < total_rows - 1) { /* Ensure we don't exceed the input area */
          //     cursorHorizontalPosition = 0; /* Reset column position after prompt */
          //     cursorVerticalPosition++; /* Move to the next row */
          //     fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition); /* Place cursor */
          //   }
          // }

          /* Add character to array */
          int rowIndex = cursorVerticalPosition - (separator_row + 1);
          if (rowIndex >= 0 && rowIndex < 2) {
            userArrayInput[rowIndex][cursorHorizontalPosition] = userTextInput[0];
            
            /* Display character on screen */
            fbputchar(userTextInput[0], cursorVerticalPosition, cursorHorizontalPosition);
            cursorHorizontalPosition++;

            /* Ensure cursor is displayed after the character */
            fbputchar('|', cursorVerticalPosition, cursorHorizontalPosition);
          }
          /* Text wrapping logic */
          if (cursorHorizontalPosition >= total_cols) { /* Check if the current row is full */
            if (cursorVerticalPosition < separator_row + 2) { /* Ensure we don't exceed the input area */
              cursorHorizontalPosition = 0; /* Reset column position after prompt */
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

// void add_message(const char message[2][MAX_MESSAGE_LENGTH]) {
//   pthread_mutex_lock(&message_mutex);

//   /* Shift all messages up by two positions to make room for new message */
//   for (int i = 0; i < MAX_MESSAGES - 2; i++) {
//     strcpy(message_buffer[i], message_buffer[i + 2]);
//   }
  
//   /* Add new message to buffer (two lines) */
//   strncpy(message_buffer[MAX_MESSAGES - 2], message[0], MAX_MESSAGE_LENGTH - 1);
//   message_buffer[MAX_MESSAGES - 2][MAX_MESSAGE_LENGTH - 1] = '\0';
  
//   strncpy(message_buffer[MAX_MESSAGES - 1], message[1], MAX_MESSAGE_LENGTH - 1);
//   message_buffer[MAX_MESSAGES - 1][MAX_MESSAGE_LENGTH - 1] = '\0';
  
//   /* Clear the display area */
//   clear_display();
  
//   /* Display all messages in the buffer with text wrapping */
//   for (int i = 0; i < MAX_MESSAGES; i++) {
//         int row = i + 1; /* +1 to skip the top border row */
//     if (row < separator_row) { /* Only display if row is above the separator */
//       int col = 0;
//       for (int j = 0; j < strlen(message_buffer[i]); j++) {
//         if (col >= total_cols) { /* Wrap to the next row if the column limit is reached */
//           row++;
//           col = 0;
//         }
//         if (row >= separator_row) break; /* Stop if we exceed the display area */
//         fbputchar(message_buffer[i][j], row, col++);
//       }
//     }
//   }
  
//   pthread_mutex_unlock(&message_mutex);
// }

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

void *network_thread_f(void *ignored) {
  char recvBuf[BUFFER_SIZE];
  int n;

  /* Receive data */
  // while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
  //   recvBuf[n] = '\0';
  //   printf("%s", recvBuf);
    
  //   /* Add prefix for received messages */
  //   char display_msg[2][MAX_MESSAGE_LENGTH];
  //   // snprintf(display_msg[0], MAX_MESSAGE_LENGTH, "Them: %s", recvBuf);
  //   snprintf(display_msg[0], MAX_MESSAGE_LENGTH, "%s", recvBuf);
  //   snprintf(display_msg[1], MAX_MESSAGE_LENGTH, " ");

  //   /* Display message*/
  //   add_message(display_msg);
  // }
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);

    char display_msg[2][MAX_MESSAGE_LENGTH];

    // Ensure the buffer is large enough to extract two messages
    if (n >= MAX_MESSAGE_LENGTH * 2) {
      strncpy(display_msg[0], recvBuf, MAX_MESSAGE_LENGTH - 1);
      display_msg[0][MAX_MESSAGE_LENGTH - 1] = '\0';

      strncpy(display_msg[1], recvBuf + MAX_MESSAGE_LENGTH, MAX_MESSAGE_LENGTH - 1);
      display_msg[1][MAX_MESSAGE_LENGTH - 1] = '\0';
    } else {
      // Handle case where the data is incomplete or malformed
      strncpy(display_msg[0], recvBuf, MAX_MESSAGE_LENGTH - 1);
      display_msg[0][MAX_MESSAGE_LENGTH - 1] = '\0';
      strncpy(display_msg[1], " ", MAX_MESSAGE_LENGTH - 1);
      display_msg[1][MAX_MESSAGE_LENGTH - 1] = '\0';
    }

    /* Display message */
    add_message(display_msg);
  }
  return NULL;
}

