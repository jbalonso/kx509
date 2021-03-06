/*
 * Copyright  �  2000
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

#include <stdio.h>
#include <stdlib.h>
#include <fstream.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <mysql.h>
#include "asn.h"
#include "dbi.h"

extern "C" {
#include "des.h"
#include "krb.h"
#include "log.h"
};

#define MAX_ENTRIES 100
#define LOG_FILE "/var/https/prod/logs/log_file"
#define CA_CERT_FILE	"/var/https/prod/ssl/ca.der"
#define CA_CLIENTCERT_FILE	"/var/https/prod/ssl/clientca.der"

typedef struct {
  char *name;
  char *val;
} tentry;

static void display_keygen_page(char *id, char *ref, int, int, const char *);
char *makeasntime(int, time_t, time_t);
static int checkkrb(char *kname, char *pass, char *inst);
static char *getval(char *aname, tentry *ent, int totlen);
static void generic_message(const char *,const char *);
ASNobject *makecert(char *, char *, ASNnetscapekey *, char *, char *);
// static void user_error(char *);
// static strpasscmp(char *, char *);
static int process_dosign_request(int, tentry *);
static int process_login_request(int, tentry *);
static int process_getcacert_request(int, tentry *);
static int process_getclientcacert_request(int, tentry *);

extern "C" {
  int umid(char *);
  int strcasecmp(...);
  void get_cn(const char *, char *);
  int strcmp(const char *, const char *);
  char *crypt(const char *, const char *);
  char *getenv(const char *);
  int setenv(const char *, const char *, int);
  int strncasecmp(...);
  char *makeword(char *line, char stop);
  char *fmakeword(FILE *f, char stop, int *len);
  char x2c(char *what);
  void unescape_url(char *url);
  void plustospace(char *str);
  int atoi(const char *);
  int rep_decode(char *, int, char *, int *);

  // Kerberos routines

  void krb_set_tkt_string(const char *);
  int krb_get_pw_in_tkt(const char *, const char *, char *, char *, char *, int, char *);
//  void des_string_to_key(const char *, des_cblock);
  void dest_tkt();
  int krb_rd_req(KTEXT, char *, char *, long, AUTH_DAT *, char *);
  int krb_mk_req(KTEXT, char *, char *, char *, long);
  int krb_get_lrealm(char *, int);
};

static char myname[50];		// Global Variable (used lotsa places)

main ()
{
    char *rn = "main";
    tentry entries[MAX_ENTRIES];
    register int x,m=0;
    int cl;
    int totent;
    char *request;

    assert(!gethostname(myname, sizeof(myname)));

    log_SetLogLevel(4);
    if (log_OpenLog(LOG_FILE, "a") == -1) {
	exit(1);
    }

//  printf("Content-type: text/html%c%c", 10, 10);

    request = getenv("QUERY_STRING");
    log_LogEvent (1, "%s: requesting %s", rn, request);

    if(strcmp(getenv("REQUEST_METHOD"),"POST")) {
	printf("Content-type: text/html%c%c", 10, 10);
	generic_message("No Posting", "This program requires a form POST");
	return (0);
    }
  
    if(strcmp(getenv("CONTENT_TYPE"),"application/x-www-form-urlencoded")) {
	printf("Content-type: text/html%c%c", 10, 10);
	printf("This script can only be used to decode form results. \n");
	return (0);
    }

    cl = atoi(getenv("CONTENT_LENGTH"));

    for(x=0;cl && (!feof(stdin));x++) {
	assert (x < MAX_ENTRIES);
	m=x;
	entries[x].val = fmakeword(stdin,'&',&cl);
	plustospace(entries[x].val);
	unescape_url(entries[x].val);
	entries[x].name = makeword(entries[x].val,'=');
	log_LogEvent (1, "%s: %s=%s", rn, entries[x].name, entries[x].val);
    }
    totent = x;
    
    log_LogEvent (1, "%s: matching request to '%s'", rn, request);
    if (!strcasecmp(request, "login"))
	return process_login_request(totent, &entries[0]);
    else if (!strcasecmp(request, "dosign")) {
	return process_dosign_request(totent, &entries[0]);
    } else if (!strcasecmp(request, "getcacert")) {
	return process_getcacert_request(totent, &entries[0]);
    } else if (!strcasecmp(request, "getclientcacert")) {
	return process_getclientcacert_request(totent, &entries[0]);
    } else {
	printf("<title>Unknown Request</Title>\n");
	printf("<H1 ALIGN=\"CENTER\">Unknown Request</H1>\n");
	printf("You requested an unknown action.<p>\n");
	return (0);
    }
}

static
void
display_keygen_page(
    char *id,
    char *ref,
    int he,
    int extra,
    const char *namestr)
{
    char *rn = "display_keygen_page";
    dbi datai;

    log_LogEvent (1, "%s: entered", rn);
    log_LogEvent (1, "%s: id='%s', ref='%s', he=%0d, extra=%0d, namestr='%s'",
	rn, id ? id : "<NULL>", ref ? ref : "<NULL>", he, extra, namestr);

    printf("Content-type: text/html%c%c", 10, 10);
    printf("<html><head><title>Login Successful</title></head>\n");
    printf("<body bgcolor=#ffffff>\n");
    printf("<h1>Login Successful</h1>\n");
    if (datai.good()) {
	char *fullname = datai.getfullname(namestr);
	printf("Welcome %s<p>\n", fullname);
    }

    printf("You have successfully logged into the Certificate Server.\n");

    printf("You will now be asked to generate your key.");
#ifdef MIT_OPTIONS
    printf(" to do this select\n");
    printf("the size of the key you wish (we suggest the longest possible)\n");
    printf("from the menu below. Note: If you are using an \"International\"\n");
    printf("version of Netscape, you will only have one choice!<p>\n");

    printf("After you select your keysize, click on the \"Do It\" button.\n");
    printf("Netscape will then lead you through the key generation process\n");
    printf("and certificate acceptance process.<p>\n");
    printf("You can also select the number of days (from now) that you would\n");
    printf("like your certificate to be valid for. The maximum allowed value\n");
    printf("is one year. If you enter a value greater then 365 days, it will be\n");
    printf("silently reduced to 365.<p>\n");
#else
    printf("To proceed, click on the \"Do It\" button.\n");
#endif /* MIT_OPTIONS */

    printf("<form method=\"POST\" action=\"https://x509.citi.umich.edu:445/cgi-bin/certcgi?dosign\">\n");
    printf("<table border=1>\n");
    printf("<tr><td>Key Size:</td><td><keygen name=userkey challenge=\"%s\"></td></tr>\n", id);
#ifdef MIT_OPTIONS
    printf("<tr><td>Certificate Lifetime (days)</td>");
    printf("<td><input name=life size=5 value=\"365\"></td></tr>\n");
#else
    printf("<td><input type=hidden name=life size=5 value=\"1\"></td></tr>\n");
#endif /* MIT_OPTIONS */
    printf("</table>\n");
    printf("<input type=hidden name=id value=\"%s\"><br>\n", id);
    if (ref) {
	printf("<input type=hidden name=ref value=\"%s\"><br>\n", ref);
    }
#ifdef MIT_OPTIONS
    if (he) {
	printf("<input type=hidden name=oextra value=\"%d\"><p>\n", extra);
	printf("<table border=0>\n");
	printf("<tr><td><input name=extra type=checkbox value=1 %s></td>",
	   extra ? "checked" : "");
	printf("<td>Check this box if, in the future when you obtain ");
	printf("certificates, you want to use your second \"secure\" password ");
	printf("instead of your Athena password. Your \"secure\" password is the");
	printf(" one that you use to access the SIS or OLSIS system.</td></tr></table><br>\n");
    } else
#endif /* MIT_OPTIONS */
	printf("<input type=hidden name=oextra value=\"0\"><br>\n");

    printf("<input type=submit value=\"Do It\">\n");
    printf("</form>\n");

    printf("</body></html>\n");
    log_LogEvent (1, "%s: exits", rn);
}

static
void
generic_message(const char *title, const char *mess)
{
    printf("Content-type: text/html%c%c", 10, 10);
    printf("<html><head><title>%s</title></head>\n", title);
    printf("<body bgcolor=#ffffff>\n");
    printf("<h1 align=\"center\">%s</h1>\n", title);
    printf("%s<p>\n", mess);
    printf("</body></html>\n");
}

static
char *
getval(char *aname, tentry *ent, int totent)
{
    int i;
    for (i = 0; i < totent; i++) {
	if (!strncasecmp(ent[i].name, aname, strlen(aname)))
	    return(ent[i].val);
    };
    return (0);
}

static
int
checkkrb(char *kname, char *pass, char *inst)
{
    char *rn="checkkrb";
    char tktstring[256];
    char realm[REALM_SZ];
    int result;
    KTEXT_ST authent;
    AUTH_DAT ad;

    sprintf(tktstring, "/tmp/tkt_cert_%ld", getpid());
    log_LogEvent (1, "%s: tktstring='%s'", rn, tktstring);
    krb_set_tkt_string(tktstring);

    result = krb_get_lrealm(realm, 1);
    if (result != KSUCCESS)
	return (0); // Hmmm.

    log_LogEvent (1, "%s: about to krb_get_pw_in_tkt", rn);
    log_LogEvent (1, "%s: kname='%s', inst='%s', realm='%s', \"cert\", \"x509\", 1, pass='%s'",
	rn, kname, inst, realm, pass);
    result = krb_get_pw_in_tkt(kname, inst, realm, "cert", "x509", 1, pass);
    log_LogEvent (1, "%s: krb_get_pw_in_tkt returns %0d", rn, result);
    if (result != KSUCCESS) {
	log_LogEvent (1, "%s: before dest_tkt", rn);
	dest_tkt();
	log_LogEvent (1, "%s: after dest_tkt", rn);
	return (result);
    }

    log_LogEvent (1, "%s: about to do krb_mk_req(..., \"cert\", \"x509\", ...)",
			rn);
    result = krb_mk_req(&authent, "cert", "x509", realm, 259);
				// The 259 is a "random" value, better then 0
    log_LogEvent (1, "%s: krb_mk_req returns %0d", rn, result);

    // Have to actually check the authent to defeat KDC spoofing
    log_LogEvent (1, "%s: about to do krb_rd_req(..., \"cert\", \"x509\", ...)",
			rn);
    result = krb_rd_req(&authent, "cert", "x509", 0, &ad,
			"/etc/srvtab.keysigner");
    log_LogEvent (1, "%s: krb_rd_req returns %0d (we want %0d)",
			rn, result, RD_AP_OK);

    dest_tkt();
    if (result == RD_AP_OK)
	return (KSUCCESS);
    return (result);
}

static
int
process_login_request(
    int totent,
    tentry entries[MAX_ENTRIES])
{
    char *rn="process_login_request";

    char *namestr = getval("name", &entries[0], totent);
    char *pass = getval("password", &entries[0], totent);
    char *ref = getval("ref", &entries[0], totent);
//billdo    char *ploginid = getval("loginid", &entries[0], totent);
    char *id;
    dbi datai;
    char loginid[15] = "123456789";
    int good = KSUCCESS;
    int extra = 0;

    if (namestr) log_LogEvent (1, "%s: namestr='%s'", rn, namestr);
    if (pass) log_LogEvent (1, "%s: pass='%s'", rn, pass);
    if (ref) log_LogEvent (1, "%s: ref='%s'", rn, ref);

    assert(namestr);
    assert(pass);
#ifdef NOTYET
    assert(ploginid);
#endif
    if (!datai.good()) {
	generic_message("Database Error",
	    "We are having problems with the server, please try again later.");	
	return (0);
    }
    good = datai.getloginidandextra(namestr, loginid, &extra);
    log_LogEvent (1, "%s: getlogin returns %0d", rn, good);
    if (!good) {
	generic_message("Cannot Find",
		"Could not find you in our database of users,"
		" please report this to keysigner@umich.edu.");
	return (0);
    }
    log_LogEvent (1, "%s: getlogin returns loginid='%s', extra=%0d",
	rn, loginid, extra);
    good = KSUCCESS;
    if (extra) {
	good = checkkrb(namestr, pass, "extra");
    } else {
	good = checkkrb(namestr, pass, "");
    }
    if (good != KSUCCESS) {
	char message[1024];
	sprintf(message, "You typed in an incorrect user name or password. %s",
		extra ?
		  "Remember, you may require your secure password to get in."
		  :
		  "");
        generic_message("Bad Password", message);		
	return (0);
    }
#ifdef NOTYET
    if (strcmp(loginid, ploginid)) {
	generic_message("Bad login ID",
		"You did not enter the correct login ID for your account.");
	return (0);
    }
#endif
    id = datai.setlogin(namestr);
    if (!id) {
	generic_message("Database Error",
	    "We are having problems with the server, please try again later.");	
	return (0);
    }
#ifdef NO_LONGER
	id="billdo";
	ref="123456";
#endif

    log_LogEvent (1, "%s: about to checkkrb", rn);
    if (checkkrb(namestr, "", "extra") == KDC_PR_UNKNOWN)
    {
	log_LogEvent (1, "%s: about to display_keygen_page(..., ref, 0, ...)",
		rn);
	display_keygen_page(id, ref, 0, extra, namestr);
	log_LogEvent (1, "%s: did display_keygen_page(..., ref, 0, ...)", rn);
    }
    else
    {
	log_LogEvent (1, "%s: about to display_keygen_page(..., ref, 1, ...)",
		rn);
	display_keygen_page(id, ref, 1, extra, namestr);
	log_LogEvent (1, "%s: did display_keygen_page(..., ref, 1, ...)", rn);
    }
    delete id;
    return (0);
}


static
int
process_dosign_request(
    int totent,
    tentry entries[MAX_ENTRIES])
{
    char *rn="process_dosign_request";

    unsigned char *der;
    unsigned int derlen;
    ASNnetscapekey *userkey;
    char *namestr;
    char emailstr[100];
    char *fullname = NULL;
    char overfullname[200];
    time_t thetime;
    int tlife;
    char *vs;
    char *ve;
    ASNobject *cert;
    char *rf;
    unsigned int i;

    log_LogEvent (1, "%s: entered", rn);

    char *id = getval("id", &entries[0], totent);
    char *keyascii = getval("userkey", &entries[0], totent);
    char *life = getval("life", &entries[0], totent);
    char *extra = getval("extra", &entries[0], totent);
    char *oextra = getval("oextra", &entries[0], totent);

    if (id) log_LogEvent(1, "%s: id='%s'", rn, id);
    if (keyascii) log_LogEvent(1, "%s: keyascii='%s'", rn, keyascii);
    if (life) log_LogEvent(1, "%s: life='%s'", rn, life);
    if (extra) log_LogEvent(1, "%s: extra='%s'", rn, extra);
    if (oextra) log_LogEvent(1, "%s: oextra='%s'", rn, oextra);

     tlife = atoi(life);

    if (!keyascii) {
	generic_message("Error",
		"Did not receive your key properly."
		" Are you using Netscape version 3.0 or greater?");
	return (0);
    }
//    assert(keyascii);
    assert(id);
    assert(oextra);

    derlen = strlen(keyascii) + 1;
    der = new unsigned char [derlen];
    log_LogEvent (1, "%s: derlen=%0d", rn, derlen);
    assert(!rep_decode(keyascii, derlen, (char *)der, (int *)&derlen));
    log_LogEvent (1, "%s: rep_decode succeeded", rn);
    userkey = ParseNetscapeKEY(der, &derlen);
    log_LogEvent (1, "%s: userkey=0x%0X", rn, userkey);
    delete der;

    if (!userkey) {
	generic_message("Error",
		"Could not complete your request. Are you using Netscape"
		" version 3.0 or greater? You are likely using Netscape"
		" version 2.X if you are seeing this message, you need"
		" to upgrade if you are.");
	return (0);
    }
//    assert(userkey);

    {
	dbi datai;

	if (!datai.good()) {
	    generic_message("Database Problems",
		"We have experienced internal database problems"
		" please try again later.");
	    return (0);
	}
	log_LogEvent (1, "%s: about to verifylogin", rn);
	namestr = datai.verifylogin(id);
	log_LogEvent (1, "%s: verifylogin returned '%s'", rn,
		namestr ? namestr : "<NULL>");
    }

    if (!namestr) {
	generic_message("Timed Out",
		"You need to submit your key with 30 minutes of going"
		" through the login process.");
	return (0);
    }

    strcpy(emailstr, namestr);
    strcat(emailstr, "@UMICH.EDU");

#ifdef FULLNAME_USING_DATABASE
    {
	dbi datai;

	if (!datai.good()) {
	    generic_message("Database Problems",
		"We have experienced internal database problems"
		" please try again later.");
	    return (0);
	}

	log_LogEvent (1, "%s: about to getfullname", rn);
	fullname = datai.getfullname(namestr);
	log_LogEvent (1, "%s: getfullname returned '%s'", rn,
		fullname ? fullname : "<NULL>");

	if (!extra && !strcmp(oextra, "1")) { // User turned off the extra flag
	    datai.setextra(namestr, 0);
	} else if (extra && !strcmp(oextra, "0")) { // User turned on the flag
	    datai.setextra(namestr, 1);
	}
    }
#else
    get_cn(namestr, overfullname);
    if (overfullname[0])
	fullname = &overfullname[0];
#endif // FULLNAME_USING_DATABASE

    if (!fullname) {
	generic_message("Cannot Find You",
		"We were unable to determine your full name,"
		" which we require to issue your certificate.");
	return (0);
    }

    (void) time(&thetime);
    tlife = atoi(life);
    if (!tlife)
	tlife = 60;	// One Minute!
    else
	tlife = tlife*24*60*60;

    vs = makeasntime(0, thetime, 920259006+(365*24*60*60));
    ve = makeasntime(tlife, thetime, 920259006+(365*24*60*60));

#ifdef ALPHA_OVERFULL
    // UMich extension -- add <e-mail_addr> to end of fullname
    sprintf(overfullname, "%s (%s UMICH.EDU)", fullname, namestr);
#else
    // UMich extension -- add <UM ID> to end of fullname
    {
	int a_umid = umid(namestr);

	if (a_umid < 0) {
	    generic_message("Cannot Find You",
		"We were unable to determine your UM ID"
		" (the pair of 4-digit numbers on your UM ID card),"
		" which we require to issue your certificate.");
	    return (0);
	}
	sprintf(overfullname, "%s (%08d)", fullname, a_umid);
    }
#endif /* ALPHA_OVERFULL */
    log_LogEvent (1, "%s: overfullname='%s'", rn, overfullname);

    log_LogEvent (1, "%s: about to makecert", rn);
    cert = makecert(overfullname, emailstr, userkey, vs, ve);
    assert(cert);
    cert->getDER(0, &derlen);
    der = new unsigned char [derlen];
    cert->getDER(der, &derlen);

    {
	dbi datai;
	if (datai.good())
	{
	    log_LogEvent (1, "%s: about to setcert", rn);
	    datai.setcert(namestr, vs, ve, (char *)der, derlen); // Log the cert
	}
    }

    printf("Content-type: application/x-x509-user-cert%c", 10);
    {
	rf = getval("ref", &entries[0], totent);
	log_LogEvent (1, "%s: getval ref returns '%s'", rn,
		rf ? rf : "<NULL>");
	if (rf)
	    printf("Refresh: 5; url=https://x509.citi.umich.edu:445/"
			"certdone.html?%s%c", rf, 10);
	else
	    printf("Refresh: 5; url=https://x509.citi.umich.edu:445/"
			"certdone.html%c", 10);
    }
    printf("Content-length: %d%c%c", derlen, 10, 10);

    for (i = 0; i < derlen; i++)
	putchar(der[i]);

    delete namestr;
    delete fullname;
    return (0);
}


static
int
process_getcacert_request(
    int totent,
    tentry entries[MAX_ENTRIES])
{
    char *rn="process_getcacert_request";

    unsigned char certder [3072];
    unsigned int i;


    log_LogEvent (1, "%s: entered", rn);

    ifstream certfile(CA_CERT_FILE, ios::in);
    certfile.read(certder, 3072);
    unsigned int certlen = certfile.gcount();

    log_LogEvent (1, "%s: certlen=%0d", rn, certlen);

    printf("Content-type: application/x-x509-ca-cert%c", 10);
    printf("Content-length: %d%c%c", certlen, 10, 10);

    for (i = 0; i < certlen; i++)
	putchar(certder[i]);

    log_LogEvent (1, "%s: returns", rn);
    return (0);
}


static
int
process_getclientcacert_request(
    int totent,
    tentry entries[MAX_ENTRIES])
{
    char *rn="process_getclientcacert_request";

    unsigned char certder [3072];
    unsigned int i;


    log_LogEvent (1, "%s: entered", rn);

    ifstream certfile(CA_CLIENTCERT_FILE, ios::in);
    certfile.read(certder, 3072);
    unsigned int certlen = certfile.gcount();

    log_LogEvent (1, "%s: certlen=%0d", rn, certlen);

    printf("Content-type: application/x-x509-ca-cert%c", 10);
    printf("Content-length: %d%c%c", certlen, 10, 10);

    for (i = 0; i < certlen; i++)
	putchar(certder[i]);

    log_LogEvent (1, "%s: returns", rn);
    return (0);
}
