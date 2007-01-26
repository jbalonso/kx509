/*
 * Copyright  �  2000,2007
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

#ifndef _INCLUDED_DLA3_H
#define _INCLUDED_DLA3_H

#define CURRENTLY_HARDCODED_QUERY_URL	"ldaps://xsamd.cc.columbia.edu/ou=fou,o=fo,c=US?serviceClass?sub?(tempid=5b5495da786f5977ff373d1ccf23341b)"

typedef struct DLA3_QUERYURL_st {
#if 1
	ASN1_OCTET_STRING *queryUrl;
#else
	char *queryUrl;
#endif
} DLA3_QUERYURL;

#endif /* _INCLUDED_DLA3_H */
