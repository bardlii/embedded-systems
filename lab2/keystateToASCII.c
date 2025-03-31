/*
 * keystateToASCII.C
 *
 * This file contains the function keystateToASCII which converts a USB keystate
 * string (formatted as "XX YY ZZ") into a single-character C string representing the ASCII
 * character corresponding to that keystate.
 *
 * The conversion is done by using sscanf() to parse exactly 2 hexadecimal digits for each value.
 * The format specifier "%2x" tells sscanf to read exactly 2 hexadecimal digits and convert them
 * into an unsigned integer. We expect to read 3 values: the modifier, the keycode, and a dummy
 * value. If sscanf does not return 3, the function returns an empty string.
 *
 * The modifier is shifted 8 bits to the left and OR’d with the keycode to produce a combined value.
 * Then a switch statement on that combined value determines the resulting ASCII character.
 *
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 
 char* keystateToASCII(const char *keystate) {
     // Allocate memory for 2 characters: one for the result and one for the null terminator.
     char* charArray = malloc(2 * sizeof(char));

     // Check if memory allocation was successful.
     if (charArray == NULL) {
         return NULL;
     }
     
     unsigned int mod, key, dummy;
     // Use sscanf to parse the keystate string.
     if (sscanf(keystate, "%2x %2x %2x", &mod, &key, &dummy) != 3) {
         charArray[0] = '\0';
         charArray[1] = '\0';
         return charArray;
     }
     
     // The modifier is shifted 8 bits left so that it occupies bits 8–15,
     // while the keycode occupies bits 0–7. The bitwise OR combines them.
     unsigned int keystateHex = (mod << 8) | key;
     
     // Switch statement on the combined keystateHex value to determine the ASCII output.
     switch (keystateHex) {
         // Special keys
         case 0x0028:  charArray[0] = '\n'; break; // "00 28 00": Enter key
         case 0x002A:  charArray[0] = '\b'; break; // "00 2A 00": Backspace
         case 0x0050:  charArray[0] = 0x11; break; // "00 50 00": Left arrow
         case 0x004F:  charArray[0] = 0x12; break; // "00 4F 00": Right arrow
             
         // Lowercase letters (modifier = 0, keycodes 0x04 to 0x1D)
         case 0x0004:  charArray[0] = 'a'; break;
         case 0x0005:  charArray[0] = 'b'; break;
         case 0x0006:  charArray[0] = 'c'; break;
         case 0x0007:  charArray[0] = 'd'; break;
         case 0x0008:  charArray[0] = 'e'; break;
         case 0x0009:  charArray[0] = 'f'; break;
         case 0x000A:  charArray[0] = 'g'; break;
         case 0x000B:  charArray[0] = 'h'; break;
         case 0x000C:  charArray[0] = 'i'; break;
         case 0x000D:  charArray[0] = 'j'; break;
         case 0x000E:  charArray[0] = 'k'; break;
         case 0x000F:  charArray[0] = 'l'; break;
         case 0x0010:  charArray[0] = 'm'; break;
         case 0x0011:  charArray[0] = 'n'; break;
         case 0x0012:  charArray[0] = 'o'; break;
         case 0x0013:  charArray[0] = 'p'; break;
         case 0x0014:  charArray[0] = 'q'; break;
         case 0x0015:  charArray[0] = 'r'; break;
         case 0x0016:  charArray[0] = 's'; break;
         case 0x0017:  charArray[0] = 't'; break;
         case 0x0018:  charArray[0] = 'u'; break;
         case 0x0019:  charArray[0] = 'v'; break;
         case 0x001A:  charArray[0] = 'w'; break;
         case 0x001B:  charArray[0] = 'x'; break;
         case 0x001C:  charArray[0] = 'y'; break;
         case 0x001D:  charArray[0] = 'z'; break;
         
         // Uppercase letters with left shift (modifier = 0x02, keycodes 0x04 to 0x1D)
         case 0x0204:  charArray[0] = 'A'; break;
         case 0x0205:  charArray[0] = 'B'; break;
         case 0x0206:  charArray[0] = 'C'; break;
         case 0x0207:  charArray[0] = 'D'; break;
         case 0x0208:  charArray[0] = 'E'; break;
         case 0x0209:  charArray[0] = 'F'; break;
         case 0x020A:  charArray[0] = 'G'; break;
         case 0x020B:  charArray[0] = 'H'; break;
         case 0x020C:  charArray[0] = 'I'; break;
         case 0x020D:  charArray[0] = 'J'; break;
         case 0x020E:  charArray[0] = 'K'; break;
         case 0x020F:  charArray[0] = 'L'; break;
         case 0x0210:  charArray[0] = 'M'; break;
         case 0x0211:  charArray[0] = 'N'; break;
         case 0x0212:  charArray[0] = 'O'; break;
         case 0x0213:  charArray[0] = 'P'; break;
         case 0x0214:  charArray[0] = 'Q'; break;
         case 0x0215:  charArray[0] = 'R'; break;
         case 0x0216:  charArray[0] = 'S'; break;
         case 0x0217:  charArray[0] = 'T'; break;
         case 0x0218:  charArray[0] = 'U'; break;
         case 0x0219:  charArray[0] = 'V'; break;
         case 0x021A:  charArray[0] = 'W'; break;
         case 0x021B:  charArray[0] = 'X'; break;
         case 0x021C:  charArray[0] = 'Y'; break;
         case 0x021D:  charArray[0] = 'Z'; break;

         // Uppercase letters with right shift (modifier = 0x20, keycodes 0x04 to 0x1D)
         case 0x2004:  charArray[0] = 'A'; break;
         case 0x2005:  charArray[0] = 'B'; break;
         case 0x2006:  charArray[0] = 'C'; break;
         case 0x2007:  charArray[0] = 'D'; break;
         case 0x2008:  charArray[0] = 'E'; break;
         case 0x2009:  charArray[0] = 'F'; break;
         case 0x200A:  charArray[0] = 'G'; break;
         case 0x200B:  charArray[0] = 'H'; break;
         case 0x200C:  charArray[0] = 'I'; break;
         case 0x200D:  charArray[0] = 'J'; break;
         case 0x200E:  charArray[0] = 'K'; break;
         case 0x200F:  charArray[0] = 'L'; break;
         case 0x2010:  charArray[0] = 'M'; break;
         case 0x2011:  charArray[0] = 'N'; break;
         case 0x2012:  charArray[0] = 'O'; break;
         case 0x2013:  charArray[0] = 'P'; break;
         case 0x2014:  charArray[0] = 'Q'; break;
         case 0x2015:  charArray[0] = 'R'; break;
         case 0x2016:  charArray[0] = 'S'; break;
         case 0x2017:  charArray[0] = 'T'; break;
         case 0x2018:  charArray[0] = 'U'; break;
         case 0x2019:  charArray[0] = 'V'; break;
         case 0x201A:  charArray[0] = 'W'; break;
         case 0x201B:  charArray[0] = 'X'; break;
         case 0x201C:  charArray[0] = 'Y'; break;
         case 0x201D:  charArray[0] = 'Z'; break;
    
         // Digits (keycodes 0x1E to 0x27 with no shift)
         case 0x001E:  charArray[0] = '1'; break;
         case 0x001F:  charArray[0] = '2'; break;
         case 0x0020:  charArray[0] = '3'; break;
         case 0x0021:  charArray[0] = '4'; break;
         case 0x0022:  charArray[0] = '5'; break;
         case 0x0023:  charArray[0] = '6'; break;
         case 0x0024:  charArray[0] = '7'; break;
         case 0x0025:  charArray[0] = '8'; break;
         case 0x0026:  charArray[0] = '9'; break;
         case 0x0027:  charArray[0] = '0'; break;
         
         // Punctuation and common symbols with no shift (modifier = 0x00)
         case 0x002C: charArray[0] = ' '; break;
         case 0x002D:  charArray[0] = '-'; break;
         case 0x002E:  charArray[0] = '='; break;
         case 0x002F:  charArray[0] = '['; break;
         case 0x0030:  charArray[0] = ']'; break;
         case 0x0031:  charArray[0] = '\\'; break;
         case 0x0033:  charArray[0] = ';'; break;
         case 0x0034:  charArray[0] = '\''; break;
         case 0x0035:  charArray[0] = '`'; break;
         case 0x0036:  charArray[0] = ','; break;
         case 0x0037:  charArray[0] = '.'; break;
         case 0x0038:  charArray[0] = '/'; break;

         // Punctuation and common symbols with right shift (modifier = 0x20)
         case 0x2035:  charArray[0] = '~'; break;
         case 0x201E:  charArray[0] = '!'; break;
         case 0x201F:  charArray[0] = '@'; break;
         case 0x2020:  charArray[0] = '#'; break;
         case 0x2021:  charArray[0] = '$'; break;
         case 0x2022:  charArray[0] = '%'; break;
         case 0x2023:  charArray[0] = '^'; break;
         case 0x2024:  charArray[0] = '&'; break;
         case 0x2025:  charArray[0] = '*'; break;
         case 0x2026:  charArray[0] = '('; break;
         case 0x2027:  charArray[0] = ')'; break;
         case 0x2036:  charArray[0] = '<'; break; // Shift + ,
         case 0x2037:  charArray[0] = '>'; break; // Shift + .
         case 0x202D:  charArray[0] = '_'; break;
         case 0x202E:  charArray[0] = '+'; break;
         case 0x202F:  charArray[0] = '{'; break;
         case 0x2030:  charArray[0] = '}'; break;
         case 0x2031:  charArray[0] = '|'; break;
         case 0x2033:  charArray[0] = ':'; break;

         // Punctuation and common symbols with left shift (modifier = 0x02)
         case 0x0235:  charArray[0] = '~'; break;
         case 0x021E:  charArray[0] = '!'; break;
         case 0x021F:  charArray[0] = '@'; break;
         case 0x0220:  charArray[0] = '#'; break;
         case 0x0221:  charArray[0] = '$'; break;
         case 0x0222:  charArray[0] = '%'; break;
         case 0x0223:  charArray[0] = '^'; break;
         case 0x0224:  charArray[0] = '&'; break;
         case 0x0225:  charArray[0] = '*'; break;
         case 0x0226:  charArray[0] = '('; break;
         case 0x0227:  charArray[0] = ')'; break;
         case 0x0236:  charArray[0] = '<'; break; // Shift + ,
         case 0x0237:  charArray[0] = '>'; break; // Shift + .
         case 0x022D:  charArray[0] = '_'; break;
         case 0x022E:  charArray[0] = '+'; break;
         case 0x022F:  charArray[0] = '{'; break;
         case 0x0230:  charArray[0] = '}'; break;
         case 0x0231:  charArray[0] = '|'; break;
         case 0x0233:  charArray[0] = ':'; break;

         default:
             // If the keystate is unrecognized, return an empty string.
             charArray[0] = '\0'; break;
     }
     
     charArray[1] = '\0';  // Null terminator string.
     return charArray;
 }
 
 