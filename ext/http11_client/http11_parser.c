#line 1 "ext/http11_client/http11_parser.rl"
/**
 * Copyright (c) 2005 Zed A. Shaw
 * You can redistribute it and/or modify it under the same terms as Ruby.
 */

#include "http11_parser.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define LEN(AT, FPC) (FPC - buffer - parser->AT)
#define MARK(M,FPC) (parser->M = (FPC) - buffer)
#define PTR_TO(F) (buffer + parser->F)
#define L(M) fprintf(stderr, "" # M "\n");


/** machine **/
#line 95 "ext/http11_client/http11_parser.rl"


/** Data **/

#line 27 "ext/http11_client/http11_parser.c"
static const int httpclient_parser_start = 0;

static const int httpclient_parser_first_final = 36;

static const int httpclient_parser_error = 1;

#line 99 "ext/http11_client/http11_parser.rl"

int httpclient_parser_init(httpclient_parser *parser)  {
  int cs = 0;
  
#line 39 "ext/http11_client/http11_parser.c"
	{
	cs = httpclient_parser_start;
	}
#line 103 "ext/http11_client/http11_parser.rl"
  parser->cs = cs;
  parser->body_start = 0;
  parser->content_len = 0;
  parser->mark = 0;
  parser->nread = 0;
  parser->field_len = 0;
  parser->field_start = 0;    

  return(1);
}


/** exec **/
size_t httpclient_parser_execute(httpclient_parser *parser, const char *buffer, size_t len, size_t off)  {
  const char *p, *pe;
  int cs = parser->cs;

  assert(off <= len && "offset past end of buffer");

  p = buffer+off;
  pe = buffer+len;

  assert(*pe == '\0' && "pointer does not end on NUL");
  assert(pe - p == len - off && "pointers aren't same distance");


  
#line 71 "ext/http11_client/http11_parser.c"
	{
	if ( p == pe )
		goto _out;
	switch ( cs )
	{
case 0:
	switch( (*p) ) {
		case 13: goto st2;
		case 48: goto tr16;
		case 59: goto st15;
		case 72: goto tr19;
	}
	if ( (*p) < 65 ) {
		if ( 49 <= (*p) && (*p) <= 57 )
			goto tr17;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr17;
	} else
		goto tr17;
	goto st1;
st1:
	goto _out1;
tr44:
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st2;
tr52:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st2;
st2:
	if ( ++p == pe )
		goto _out2;
case 2:
#line 117 "ext/http11_client/http11_parser.c"
	if ( (*p) == 10 )
		goto tr34;
	goto st1;
tr33:
#line 57 "ext/http11_client/http11_parser.rl"
	{ 
    parser->body_start = p - buffer + 1; 
    if(parser->header_done != NULL)
      parser->header_done(parser->data, p + 1, pe - p - 1);
    goto _out36;
  }
	goto st36;
tr34:
#line 53 "ext/http11_client/http11_parser.rl"
	{
    parser->last_chunk(parser->data, NULL, 0);
  }
#line 57 "ext/http11_client/http11_parser.rl"
	{ 
    parser->body_start = p - buffer + 1; 
    if(parser->header_done != NULL)
      parser->header_done(parser->data, p + 1, pe - p - 1);
    goto _out36;
  }
	goto st36;
tr35:
#line 57 "ext/http11_client/http11_parser.rl"
	{ 
    parser->body_start = p - buffer + 1; 
    if(parser->header_done != NULL)
      parser->header_done(parser->data, p + 1, pe - p - 1);
    goto _out36;
  }
#line 53 "ext/http11_client/http11_parser.rl"
	{
    parser->last_chunk(parser->data, NULL, 0);
  }
	goto st36;
st36:
	if ( ++p == pe )
		goto _out36;
case 36:
#line 160 "ext/http11_client/http11_parser.c"
	goto st1;
tr16:
#line 23 "ext/http11_client/http11_parser.rl"
	{MARK(mark, p); }
	goto st3;
st3:
	if ( ++p == pe )
		goto _out3;
case 3:
#line 170 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr50;
		case 59: goto tr51;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st5;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st5;
	} else
		goto st5;
	goto st1;
tr47:
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st4;
tr50:
#line 49 "ext/http11_client/http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st4;
tr56:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st4;
st4:
	if ( ++p == pe )
		goto _out4;
case 4:
#line 212 "ext/http11_client/http11_parser.c"
	if ( (*p) == 10 )
		goto tr35;
	goto st1;
tr17:
#line 23 "ext/http11_client/http11_parser.rl"
	{MARK(mark, p); }
	goto st5;
st5:
	if ( ++p == pe )
		goto _out5;
case 5:
#line 224 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr25;
		case 59: goto tr27;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto st5;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto st5;
	} else
		goto st5;
	goto st1;
tr22:
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st6;
tr25:
#line 49 "ext/http11_client/http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st6;
tr28:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st6;
st6:
	if ( ++p == pe )
		goto _out6;
case 6:
#line 266 "ext/http11_client/http11_parser.c"
	if ( (*p) == 10 )
		goto tr33;
	goto st1;
tr24:
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st7;
tr27:
#line 49 "ext/http11_client/http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st7;
tr30:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st7;
st7:
	if ( ++p == pe )
		goto _out7;
case 7:
#line 298 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr9;
		case 124: goto tr9;
		case 126: goto tr9;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr9;
		} else if ( (*p) >= 35 )
			goto tr9;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr9;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr9;
		} else
			goto tr9;
	} else
		goto tr9;
	goto st1;
tr9:
#line 25 "ext/http11_client/http11_parser.rl"
	{ MARK(field_start, p); }
	goto st8;
st8:
	if ( ++p == pe )
		goto _out8;
case 8:
#line 330 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr28;
		case 33: goto st8;
		case 59: goto tr30;
		case 61: goto tr31;
		case 124: goto st8;
		case 126: goto st8;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st8;
		} else if ( (*p) >= 35 )
			goto st8;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st8;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st8;
		} else
			goto st8;
	} else
		goto st8;
	goto st1;
tr31:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st9;
st9:
	if ( ++p == pe )
		goto _out9;
case 9:
#line 369 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr10;
		case 124: goto tr10;
		case 126: goto tr10;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr10;
		} else if ( (*p) >= 35 )
			goto tr10;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr10;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr10;
		} else
			goto tr10;
	} else
		goto tr10;
	goto st1;
tr10:
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st10;
st10:
	if ( ++p == pe )
		goto _out10;
case 10:
#line 401 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr22;
		case 33: goto st10;
		case 59: goto tr24;
		case 124: goto st10;
		case 126: goto st10;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st10;
		} else if ( (*p) >= 35 )
			goto st10;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st10;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st10;
		} else
			goto st10;
	} else
		goto st10;
	goto st1;
tr49:
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st11;
tr51:
#line 49 "ext/http11_client/http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st11;
tr58:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st11;
st11:
	if ( ++p == pe )
		goto _out11;
case 11:
#line 455 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr63;
		case 124: goto tr63;
		case 126: goto tr63;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr63;
		} else if ( (*p) >= 35 )
			goto tr63;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr63;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr63;
		} else
			goto tr63;
	} else
		goto tr63;
	goto st1;
tr63:
#line 25 "ext/http11_client/http11_parser.rl"
	{ MARK(field_start, p); }
	goto st12;
st12:
	if ( ++p == pe )
		goto _out12;
case 12:
#line 487 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr56;
		case 33: goto st12;
		case 59: goto tr58;
		case 61: goto tr59;
		case 124: goto st12;
		case 126: goto st12;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st12;
		} else if ( (*p) >= 35 )
			goto st12;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st12;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st12;
		} else
			goto st12;
	} else
		goto st12;
	goto st1;
tr59:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st13;
st13:
	if ( ++p == pe )
		goto _out13;
case 13:
#line 526 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr61;
		case 124: goto tr61;
		case 126: goto tr61;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr61;
		} else if ( (*p) >= 35 )
			goto tr61;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr61;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr61;
		} else
			goto tr61;
	} else
		goto tr61;
	goto st1;
tr61:
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st14;
st14:
	if ( ++p == pe )
		goto _out14;
case 14:
#line 558 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr47;
		case 33: goto st14;
		case 59: goto tr49;
		case 124: goto st14;
		case 126: goto st14;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st14;
		} else if ( (*p) >= 35 )
			goto st14;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st14;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st14;
		} else
			goto st14;
	} else
		goto st14;
	goto st1;
tr46:
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st15;
tr54:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st15;
st15:
	if ( ++p == pe )
		goto _out15;
case 15:
#line 606 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr62;
		case 124: goto tr62;
		case 126: goto tr62;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr62;
		} else if ( (*p) >= 35 )
			goto tr62;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr62;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr62;
		} else
			goto tr62;
	} else
		goto tr62;
	goto st1;
tr62:
#line 25 "ext/http11_client/http11_parser.rl"
	{ MARK(field_start, p); }
	goto st16;
st16:
	if ( ++p == pe )
		goto _out16;
case 16:
#line 638 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr52;
		case 33: goto st16;
		case 59: goto tr54;
		case 61: goto tr55;
		case 124: goto st16;
		case 126: goto st16;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st16;
		} else if ( (*p) >= 35 )
			goto st16;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st16;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st16;
		} else
			goto st16;
	} else
		goto st16;
	goto st1;
tr55:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st17;
st17:
	if ( ++p == pe )
		goto _out17;
case 17:
#line 677 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr60;
		case 124: goto tr60;
		case 126: goto tr60;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr60;
		} else if ( (*p) >= 35 )
			goto tr60;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr60;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr60;
		} else
			goto tr60;
	} else
		goto tr60;
	goto st1;
tr60:
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st18;
st18:
	if ( ++p == pe )
		goto _out18;
case 18:
#line 709 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr44;
		case 33: goto st18;
		case 59: goto tr46;
		case 124: goto st18;
		case 126: goto st18;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st18;
		} else if ( (*p) >= 35 )
			goto st18;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st18;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st18;
		} else
			goto st18;
	} else
		goto st18;
	goto st1;
tr19:
#line 23 "ext/http11_client/http11_parser.rl"
	{MARK(mark, p); }
	goto st19;
st19:
	if ( ++p == pe )
		goto _out19;
case 19:
#line 743 "ext/http11_client/http11_parser.c"
	if ( (*p) == 84 )
		goto st20;
	goto st1;
st20:
	if ( ++p == pe )
		goto _out20;
case 20:
	if ( (*p) == 84 )
		goto st21;
	goto st1;
st21:
	if ( ++p == pe )
		goto _out21;
case 21:
	if ( (*p) == 80 )
		goto st22;
	goto st1;
st22:
	if ( ++p == pe )
		goto _out22;
case 22:
	if ( (*p) == 47 )
		goto st23;
	goto st1;
st23:
	if ( ++p == pe )
		goto _out23;
case 23:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st24;
	goto st1;
st24:
	if ( ++p == pe )
		goto _out24;
case 24:
	if ( (*p) == 46 )
		goto st25;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st24;
	goto st1;
st25:
	if ( ++p == pe )
		goto _out25;
case 25:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st26;
	goto st1;
st26:
	if ( ++p == pe )
		goto _out26;
case 26:
	if ( (*p) == 32 )
		goto tr13;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st26;
	goto st1;
tr13:
#line 45 "ext/http11_client/http11_parser.rl"
	{	
    parser->http_version(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st27;
st27:
	if ( ++p == pe )
		goto _out27;
case 27:
#line 810 "ext/http11_client/http11_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr4;
	goto st1;
tr4:
#line 23 "ext/http11_client/http11_parser.rl"
	{MARK(mark, p); }
	goto st28;
st28:
	if ( ++p == pe )
		goto _out28;
case 28:
#line 822 "ext/http11_client/http11_parser.c"
	if ( (*p) == 32 )
		goto tr11;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st28;
	goto st1;
tr11:
#line 41 "ext/http11_client/http11_parser.rl"
	{ 
    parser->status_code(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st29;
st29:
	if ( ++p == pe )
		goto _out29;
case 29:
#line 838 "ext/http11_client/http11_parser.c"
	goto tr40;
tr40:
#line 23 "ext/http11_client/http11_parser.rl"
	{MARK(mark, p); }
	goto st30;
st30:
	if ( ++p == pe )
		goto _out30;
case 30:
#line 848 "ext/http11_client/http11_parser.c"
	if ( (*p) == 13 )
		goto tr39;
	goto st30;
tr37:
#line 33 "ext/http11_client/http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st31;
tr39:
#line 37 "ext/http11_client/http11_parser.rl"
	{ 
    parser->reason_phrase(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st31;
st31:
	if ( ++p == pe )
		goto _out31;
case 31:
#line 868 "ext/http11_client/http11_parser.c"
	if ( (*p) == 10 )
		goto st32;
	goto st1;
st32:
	if ( ++p == pe )
		goto _out32;
case 32:
	switch( (*p) ) {
		case 13: goto st6;
		case 33: goto tr21;
		case 124: goto tr21;
		case 126: goto tr21;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr21;
		} else if ( (*p) >= 35 )
			goto tr21;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr21;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr21;
		} else
			goto tr21;
	} else
		goto tr21;
	goto st1;
tr21:
#line 25 "ext/http11_client/http11_parser.rl"
	{ MARK(field_start, p); }
	goto st33;
st33:
	if ( ++p == pe )
		goto _out33;
case 33:
#line 908 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 33: goto st33;
		case 58: goto tr8;
		case 124: goto st33;
		case 126: goto st33;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st33;
		} else if ( (*p) >= 35 )
			goto st33;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st33;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st33;
		} else
			goto st33;
	} else
		goto st33;
	goto st1;
tr8:
#line 27 "ext/http11_client/http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
	goto st34;
tr42:
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st34;
st34:
	if ( ++p == pe )
		goto _out34;
case 34:
#line 947 "ext/http11_client/http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr37;
		case 32: goto tr42;
	}
	goto tr41;
tr41:
#line 31 "ext/http11_client/http11_parser.rl"
	{ MARK(mark, p); }
	goto st35;
st35:
	if ( ++p == pe )
		goto _out35;
case 35:
#line 961 "ext/http11_client/http11_parser.c"
	if ( (*p) == 13 )
		goto tr37;
	goto st35;
	}
	_out1: cs = 1; goto _out; 
	_out2: cs = 2; goto _out; 
	_out36: cs = 36; goto _out; 
	_out3: cs = 3; goto _out; 
	_out4: cs = 4; goto _out; 
	_out5: cs = 5; goto _out; 
	_out6: cs = 6; goto _out; 
	_out7: cs = 7; goto _out; 
	_out8: cs = 8; goto _out; 
	_out9: cs = 9; goto _out; 
	_out10: cs = 10; goto _out; 
	_out11: cs = 11; goto _out; 
	_out12: cs = 12; goto _out; 
	_out13: cs = 13; goto _out; 
	_out14: cs = 14; goto _out; 
	_out15: cs = 15; goto _out; 
	_out16: cs = 16; goto _out; 
	_out17: cs = 17; goto _out; 
	_out18: cs = 18; goto _out; 
	_out19: cs = 19; goto _out; 
	_out20: cs = 20; goto _out; 
	_out21: cs = 21; goto _out; 
	_out22: cs = 22; goto _out; 
	_out23: cs = 23; goto _out; 
	_out24: cs = 24; goto _out; 
	_out25: cs = 25; goto _out; 
	_out26: cs = 26; goto _out; 
	_out27: cs = 27; goto _out; 
	_out28: cs = 28; goto _out; 
	_out29: cs = 29; goto _out; 
	_out30: cs = 30; goto _out; 
	_out31: cs = 31; goto _out; 
	_out32: cs = 32; goto _out; 
	_out33: cs = 33; goto _out; 
	_out34: cs = 34; goto _out; 
	_out35: cs = 35; goto _out; 

	_out: {}
	}
#line 130 "ext/http11_client/http11_parser.rl"

  parser->cs = cs;
  parser->nread += p - (buffer + off);

  assert(p <= pe && "buffer overflow after parsing execute");
  assert(parser->nread <= len && "nread longer than length");
  assert(parser->body_start <= len && "body starts after buffer end");
  assert(parser->mark < len && "mark is after buffer end");
  assert(parser->field_len <= len && "field has length longer than whole buffer");
  assert(parser->field_start < len && "field starts after buffer end");

  if(parser->body_start) {
    /* final \r\n combo encountered so stop right here */
    
#line 1020 "ext/http11_client/http11_parser.c"
#line 144 "ext/http11_client/http11_parser.rl"
    parser->nread++;
  }

  return(parser->nread);
}

int httpclient_parser_finish(httpclient_parser *parser)
{
  int cs = parser->cs;

  
#line 1033 "ext/http11_client/http11_parser.c"
#line 155 "ext/http11_client/http11_parser.rl"

  parser->cs = cs;

  if (httpclient_parser_has_error(parser) ) {
    return -1;
  } else if (httpclient_parser_is_finished(parser) ) {
    return 1;
  } else {
    return 0;
  }
}

int httpclient_parser_has_error(httpclient_parser *parser) {
  return parser->cs == httpclient_parser_error;
}

int httpclient_parser_is_finished(httpclient_parser *parser) {
  return parser->cs == httpclient_parser_first_final;
}
