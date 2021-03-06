/*
 * Copyright  ©  2007
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * permission is granted to use, copy, create derivative works 
 * and redistribute this software and such derivative works 
 * for any purpose, so long as the name of the university of 
 * michigan is not used in any advertising or publicity 
 * pertaining to the use or distribution of this software 
 * without specific, written prior authorization.  if the 
 * above copyright notice or any other identification of the 
 * university of michigan is included in any copy of any 
 * portion of this software, then the disclaimer below must 
 * also be included.
 *
 * this software is provided as is, without representation 
 * from the university of michigan as to its fitness for any 
 * purpose, and without warranty by the university of 
 * michigan of any kind, either express or implied, including 
 * without limitation the implied warranties of 
 * merchantability and fitness for a particular purpose. the 
 * regents of the university of michigan shall not be liable 
 * for any damages, including special, indirect, incidental, or 
 * consequential damages, with respect to any claim arising 
 * out of or in connection with the use of the software, even 
 * if it has been or is hereafter advised of the possibility of 
 * such damages.
 */


/* self contained dialog box routine to 
 * read a user@realm and password
 */
#include <windows.h>

typedef struct dialog_u_pw_params {
	int	   rc;
    char * caption;			// Title for the dialog box
    char * promptuser;		// Static text above user edit box
    char * user;			// Location and starting user name
    int    userlen;			// Max size of above
    char * promptpassword;	// Static text above password box
    char * password;		// location to receive password
    int    passwordlen;		// Max length of above
} dialog_u_pw_params ;

int
read_user_password_dialog(HINSTANCE hInstance,
						  HWND hwndParent,
						  dialog_u_pw_params * dupwp);

