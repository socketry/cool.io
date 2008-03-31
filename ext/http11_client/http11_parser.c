#line 1 "http11_parser.rl"
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
#line 95 "http11_parser.rl"


/** Data **/

#line 27 "http11_parser.c"
static const int httpclient_parser_start = 1;
static const int httpclient_parser_first_final = 37;
static const int httpclient_parser_error = 0;

static const int httpclient_parser_en_main = 1;

#line 99 "http11_parser.rl"

int httpclient_parser_init(httpclient_parser *parser)  {
  int cs = 0;
  
#line 39 "http11_parser.c"
	{
	cs = httpclient_parser_start;
	}
#line 103 "http11_parser.rl"
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


  
#line 71 "http11_parser.c"
	{
	if ( p == pe )
		goto _out;
	switch ( cs )
	{
case 1:
	switch( (*p) ) {
		case 13: goto st2;
		case 48: goto tr2;
		case 59: goto st16;
		case 72: goto tr5;
	}
	if ( (*p) < 65 ) {
		if ( 49 <= (*p) && (*p) <= 57 )
			goto tr3;
	} else if ( (*p) > 70 ) {
		if ( 97 <= (*p) && (*p) <= 102 )
			goto tr3;
	} else
		goto tr3;
	goto st0;
st0:
	goto _out0;
tr38:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st2;
tr43:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st2;
st2:
	if ( ++p == pe )
		goto _out2;
case 2:
#line 117 "http11_parser.c"
	if ( (*p) == 10 )
		goto tr6;
	goto st0;
tr6:
#line 53 "http11_parser.rl"
	{
    parser->last_chunk(parser->data, NULL, 0);
  }
#line 57 "http11_parser.rl"
	{ 
    parser->body_start = p - buffer + 1; 
    if(parser->header_done != NULL)
      parser->header_done(parser->data, p + 1, pe - p - 1);
    goto _out37;
  }
	goto st37;
tr13:
#line 57 "http11_parser.rl"
	{ 
    parser->body_start = p - buffer + 1; 
    if(parser->header_done != NULL)
      parser->header_done(parser->data, p + 1, pe - p - 1);
    goto _out37;
  }
	goto st37;
tr14:
#line 57 "http11_parser.rl"
	{ 
    parser->body_start = p - buffer + 1; 
    if(parser->header_done != NULL)
      parser->header_done(parser->data, p + 1, pe - p - 1);
    goto _out37;
  }
#line 53 "http11_parser.rl"
	{
    parser->last_chunk(parser->data, NULL, 0);
  }
	goto st37;
st37:
	if ( ++p == pe )
		goto _out37;
case 37:
#line 160 "http11_parser.c"
	goto st0;
tr2:
#line 23 "http11_parser.rl"
	{MARK(mark, p); }
	goto st3;
st3:
	if ( ++p == pe )
		goto _out3;
case 3:
#line 170 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr8;
		case 32: goto tr7;
		case 59: goto tr10;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 12 )
			goto tr7;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 70 ) {
			if ( 97 <= (*p) && (*p) <= 102 )
				goto st7;
		} else if ( (*p) >= 65 )
			goto st7;
	} else
		goto st7;
	goto st0;
tr7:
#line 49 "http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st4;
tr18:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st4;
tr24:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st4;
st4:
	if ( ++p == pe )
		goto _out4;
case 4:
#line 216 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto st5;
		case 32: goto st4;
	}
	if ( 9 <= (*p) && (*p) <= 12 )
		goto st4;
	goto st0;
tr15:
#line 49 "http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st5;
tr19:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st5;
tr25:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st5;
st5:
	if ( ++p == pe )
		goto _out5;
case 5:
#line 252 "http11_parser.c"
	if ( (*p) == 10 )
		goto tr13;
	goto st0;
tr8:
#line 49 "http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st6;
tr29:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st6;
tr34:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st6;
st6:
	if ( ++p == pe )
		goto _out6;
case 6:
#line 284 "http11_parser.c"
	if ( (*p) == 10 )
		goto tr14;
	goto st0;
tr3:
#line 23 "http11_parser.rl"
	{MARK(mark, p); }
	goto st7;
st7:
	if ( ++p == pe )
		goto _out7;
case 7:
#line 296 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr15;
		case 32: goto tr7;
		case 59: goto tr16;
	}
	if ( (*p) < 48 ) {
		if ( 9 <= (*p) && (*p) <= 12 )
			goto tr7;
	} else if ( (*p) > 57 ) {
		if ( (*p) > 70 ) {
			if ( 97 <= (*p) && (*p) <= 102 )
				goto st7;
		} else if ( (*p) >= 65 )
			goto st7;
	} else
		goto st7;
	goto st0;
tr16:
#line 49 "http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st8;
tr21:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st8;
tr27:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st8;
st8:
	if ( ++p == pe )
		goto _out8;
case 8:
#line 342 "http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr17;
		case 124: goto tr17;
		case 126: goto tr17;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr17;
		} else if ( (*p) >= 35 )
			goto tr17;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr17;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr17;
		} else
			goto tr17;
	} else
		goto tr17;
	goto st0;
tr17:
#line 25 "http11_parser.rl"
	{ MARK(field_start, p); }
	goto st9;
st9:
	if ( ++p == pe )
		goto _out9;
case 9:
#line 374 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr19;
		case 32: goto tr18;
		case 33: goto st9;
		case 59: goto tr21;
		case 61: goto tr22;
		case 124: goto st9;
		case 126: goto st9;
	}
	if ( (*p) < 45 ) {
		if ( (*p) < 35 ) {
			if ( 9 <= (*p) && (*p) <= 12 )
				goto tr18;
		} else if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st9;
		} else
			goto st9;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st9;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st9;
		} else
			goto st9;
	} else
		goto st9;
	goto st0;
tr22:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st10;
st10:
	if ( ++p == pe )
		goto _out10;
case 10:
#line 417 "http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr23;
		case 124: goto tr23;
		case 126: goto tr23;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr23;
		} else if ( (*p) >= 35 )
			goto tr23;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr23;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr23;
		} else
			goto tr23;
	} else
		goto tr23;
	goto st0;
tr23:
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st11;
st11:
	if ( ++p == pe )
		goto _out11;
case 11:
#line 449 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr25;
		case 32: goto tr24;
		case 33: goto st11;
		case 59: goto tr27;
		case 124: goto st11;
		case 126: goto st11;
	}
	if ( (*p) < 45 ) {
		if ( (*p) < 35 ) {
			if ( 9 <= (*p) && (*p) <= 12 )
				goto tr24;
		} else if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st11;
		} else
			goto st11;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st11;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st11;
		} else
			goto st11;
	} else
		goto st11;
	goto st0;
tr10:
#line 49 "http11_parser.rl"
	{
    parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st12;
tr31:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st12;
tr36:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st12;
st12:
	if ( ++p == pe )
		goto _out12;
case 12:
#line 507 "http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr28;
		case 124: goto tr28;
		case 126: goto tr28;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr28;
		} else if ( (*p) >= 35 )
			goto tr28;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr28;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr28;
		} else
			goto tr28;
	} else
		goto tr28;
	goto st0;
tr28:
#line 25 "http11_parser.rl"
	{ MARK(field_start, p); }
	goto st13;
st13:
	if ( ++p == pe )
		goto _out13;
case 13:
#line 539 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr29;
		case 32: goto tr18;
		case 33: goto st13;
		case 59: goto tr31;
		case 61: goto tr32;
		case 124: goto st13;
		case 126: goto st13;
	}
	if ( (*p) < 45 ) {
		if ( (*p) < 35 ) {
			if ( 9 <= (*p) && (*p) <= 12 )
				goto tr18;
		} else if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st13;
		} else
			goto st13;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st13;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st13;
		} else
			goto st13;
	} else
		goto st13;
	goto st0;
tr32:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st14;
st14:
	if ( ++p == pe )
		goto _out14;
case 14:
#line 582 "http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr33;
		case 124: goto tr33;
		case 126: goto tr33;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr33;
		} else if ( (*p) >= 35 )
			goto tr33;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr33;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr33;
		} else
			goto tr33;
	} else
		goto tr33;
	goto st0;
tr33:
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st15;
st15:
	if ( ++p == pe )
		goto _out15;
case 15:
#line 614 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr34;
		case 32: goto tr24;
		case 33: goto st15;
		case 59: goto tr36;
		case 124: goto st15;
		case 126: goto st15;
	}
	if ( (*p) < 45 ) {
		if ( (*p) < 35 ) {
			if ( 9 <= (*p) && (*p) <= 12 )
				goto tr24;
		} else if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st15;
		} else
			goto st15;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st15;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st15;
		} else
			goto st15;
	} else
		goto st15;
	goto st0;
tr40:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st16;
tr45:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st16;
st16:
	if ( ++p == pe )
		goto _out16;
case 16:
#line 666 "http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr37;
		case 124: goto tr37;
		case 126: goto tr37;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr37;
		} else if ( (*p) >= 35 )
			goto tr37;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr37;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr37;
		} else
			goto tr37;
	} else
		goto tr37;
	goto st0;
tr37:
#line 25 "http11_parser.rl"
	{ MARK(field_start, p); }
	goto st17;
st17:
	if ( ++p == pe )
		goto _out17;
case 17:
#line 698 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr38;
		case 33: goto st17;
		case 59: goto tr40;
		case 61: goto tr41;
		case 124: goto st17;
		case 126: goto st17;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st17;
		} else if ( (*p) >= 35 )
			goto st17;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st17;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st17;
		} else
			goto st17;
	} else
		goto st17;
	goto st0;
tr41:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st18;
st18:
	if ( ++p == pe )
		goto _out18;
case 18:
#line 737 "http11_parser.c"
	switch( (*p) ) {
		case 33: goto tr42;
		case 124: goto tr42;
		case 126: goto tr42;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto tr42;
		} else if ( (*p) >= 35 )
			goto tr42;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto tr42;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto tr42;
		} else
			goto tr42;
	} else
		goto tr42;
	goto st0;
tr42:
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st19;
st19:
	if ( ++p == pe )
		goto _out19;
case 19:
#line 769 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr43;
		case 33: goto st19;
		case 59: goto tr45;
		case 124: goto st19;
		case 126: goto st19;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st19;
		} else if ( (*p) >= 35 )
			goto st19;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st19;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st19;
		} else
			goto st19;
	} else
		goto st19;
	goto st0;
tr5:
#line 23 "http11_parser.rl"
	{MARK(mark, p); }
	goto st20;
st20:
	if ( ++p == pe )
		goto _out20;
case 20:
#line 803 "http11_parser.c"
	if ( (*p) == 84 )
		goto st21;
	goto st0;
st21:
	if ( ++p == pe )
		goto _out21;
case 21:
	if ( (*p) == 84 )
		goto st22;
	goto st0;
st22:
	if ( ++p == pe )
		goto _out22;
case 22:
	if ( (*p) == 80 )
		goto st23;
	goto st0;
st23:
	if ( ++p == pe )
		goto _out23;
case 23:
	if ( (*p) == 47 )
		goto st24;
	goto st0;
st24:
	if ( ++p == pe )
		goto _out24;
case 24:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st25;
	goto st0;
st25:
	if ( ++p == pe )
		goto _out25;
case 25:
	if ( (*p) == 46 )
		goto st26;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st25;
	goto st0;
st26:
	if ( ++p == pe )
		goto _out26;
case 26:
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st27;
	goto st0;
st27:
	if ( ++p == pe )
		goto _out27;
case 27:
	if ( (*p) == 32 )
		goto tr53;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st27;
	goto st0;
tr53:
#line 45 "http11_parser.rl"
	{	
    parser->http_version(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st28;
st28:
	if ( ++p == pe )
		goto _out28;
case 28:
#line 870 "http11_parser.c"
	if ( 48 <= (*p) && (*p) <= 57 )
		goto tr54;
	goto st0;
tr54:
#line 23 "http11_parser.rl"
	{MARK(mark, p); }
	goto st29;
st29:
	if ( ++p == pe )
		goto _out29;
case 29:
#line 882 "http11_parser.c"
	if ( (*p) == 32 )
		goto tr55;
	if ( 48 <= (*p) && (*p) <= 57 )
		goto st29;
	goto st0;
tr55:
#line 41 "http11_parser.rl"
	{ 
    parser->status_code(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st30;
st30:
	if ( ++p == pe )
		goto _out30;
case 30:
#line 898 "http11_parser.c"
	goto tr57;
tr57:
#line 23 "http11_parser.rl"
	{MARK(mark, p); }
	goto st31;
st31:
	if ( ++p == pe )
		goto _out31;
case 31:
#line 908 "http11_parser.c"
	if ( (*p) == 13 )
		goto tr59;
	goto st31;
tr65:
#line 33 "http11_parser.rl"
	{ 
    parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, p));
  }
	goto st32;
tr59:
#line 37 "http11_parser.rl"
	{ 
    parser->reason_phrase(parser->data, PTR_TO(mark), LEN(mark, p));
  }
	goto st32;
st32:
	if ( ++p == pe )
		goto _out32;
case 32:
#line 928 "http11_parser.c"
	if ( (*p) == 10 )
		goto st33;
	goto st0;
st33:
	if ( ++p == pe )
		goto _out33;
case 33:
	switch( (*p) ) {
		case 13: goto st5;
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
	goto st0;
tr61:
#line 25 "http11_parser.rl"
	{ MARK(field_start, p); }
	goto st34;
st34:
	if ( ++p == pe )
		goto _out34;
case 34:
#line 968 "http11_parser.c"
	switch( (*p) ) {
		case 33: goto st34;
		case 58: goto tr63;
		case 124: goto st34;
		case 126: goto st34;
	}
	if ( (*p) < 45 ) {
		if ( (*p) > 39 ) {
			if ( 42 <= (*p) && (*p) <= 43 )
				goto st34;
		} else if ( (*p) >= 35 )
			goto st34;
	} else if ( (*p) > 46 ) {
		if ( (*p) < 65 ) {
			if ( 48 <= (*p) && (*p) <= 57 )
				goto st34;
		} else if ( (*p) > 90 ) {
			if ( 94 <= (*p) && (*p) <= 122 )
				goto st34;
		} else
			goto st34;
	} else
		goto st34;
	goto st0;
tr66:
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st35;
tr63:
#line 27 "http11_parser.rl"
	{ 
    parser->field_len = LEN(field_start, p);
  }
	goto st35;
st35:
	if ( ++p == pe )
		goto _out35;
case 35:
#line 1007 "http11_parser.c"
	switch( (*p) ) {
		case 13: goto tr65;
		case 32: goto tr66;
	}
	goto tr64;
tr64:
#line 31 "http11_parser.rl"
	{ MARK(mark, p); }
	goto st36;
st36:
	if ( ++p == pe )
		goto _out36;
case 36:
#line 1021 "http11_parser.c"
	if ( (*p) == 13 )
		goto tr65;
	goto st36;
	}
	_out0: cs = 0; goto _out; 
	_out2: cs = 2; goto _out; 
	_out37: cs = 37; goto _out; 
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
	_out36: cs = 36; goto _out; 

	_out: {}
	}
#line 130 "http11_parser.rl"

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
    
#line 1081 "http11_parser.c"
#line 144 "http11_parser.rl"
    parser->nread++;
  }

  return(parser->nread);
}

int httpclient_parser_finish(httpclient_parser *parser)
{
  int cs = parser->cs;

  
#line 1094 "http11_parser.c"
#line 155 "http11_parser.rl"

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
