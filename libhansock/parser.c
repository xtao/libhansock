/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdlib.h>

#include "common.h"
#include "alloc.h"
#include "parser.h"

#define MARK rp->mark = rp->p

struct _ReplyParser
{
    size_t p; //position
    int cs; //state
    Reply *reply;
    size_t mark; //helper to mark start of interesting data
};

void ReplyParser_reset(ReplyParser *rp)
{
    rp->p = 0;
    rp->cs = 0;
    rp->mark = 0;
    rp->reply = NULL;
}

ReplyParser *ReplyParser_new()
{
    DEBUG(("alloc ReplyParser\n"));
    ReplyParser *rp = Alloc_alloc_T(ReplyParser);
    if(rp == NULL) {
        Module_set_error(GET_MODULE(), "Out of memory while allocating ReplyParser");
        return NULL;
    }
    ReplyParser_reset(rp);
    return rp;
}

void ReplyParser_free(ReplyParser *rp)
{
    if(rp == NULL) {
        return;
    }
    DEBUG(("dealloc ReplyParser\n"));
    Alloc_free_T(rp, ReplyParser);
}


/**
 * A State machine for parsing replies.
 */
ReplyParserResult ReplyParser_execute(ReplyParser *rp, const char *data, size_t len, Reply **reply)
{
    DEBUG(("enter rp exec, rp->p: %d, len: %d, cs: %d\n", rp->p, len, rp->cs));
    assert(rp->p <= len);
    *reply = NULL;
    while((rp->p) < len) {
        Byte c = data[rp->p];
        //printf("cs: %d, char: %d\n", rp->cs, c);
        switch(rp->cs) {
            case 0: { //initial state
                assert(rp->reply == NULL);
                //rp->reply = Reply_new();
                //fall trough
            }
            case 1: { //normal state
                if(c >= 0x10 && c <= 0xff) {
                    //NORMAL char, most common
                    rp->p++;
                    //cs stays 1
                    continue;
                }
                else if(c == 0x09) { //TAB
                    rp->p++;
                    //cs stays 1
                    continue;
                }
                else if(c == 0x0A) { //EOL
                    rp->p++;
                    rp->cs = 4;
                    continue;
                }
                else if(c == 0x00) { //NULL
                    rp->p++;
                    rp->cs = 2;
                    continue;
                }
                else if(c == 0x01) { //encoded char
                    rp->p++;
                    rp->cs = 3;
                    continue;
                }
                break;
            }
            case 2: {
                // after reading a NULL
                // here we expect either a TAB or EOL
                if(c == 0x09) { //TAB
                    rp->p++;
                    rp->cs = 1;
                    continue;
                }
                else if(c == 0x0A){ //EOL
                    rp->p++;
                    rp->cs = 4;
                    continue;
                }
                break;
            }
            case 3: {
                // next char should be encoded char
                if(c >= 0x40 && c <= 0x4f) {
                    //OK
                    rp->p++;
                    rp->cs = 1;
                    continue;
                }
                break;
            }
            case 4: { //End of Line
                break;
            }
        }
        return RPR_ERROR;
    }
    DEBUG(("exit rp pos: %d len: %d cs: %d\n", rp->p, len, rp->cs));
    assert(rp->p == len);
    return RPR_MORE;
}

