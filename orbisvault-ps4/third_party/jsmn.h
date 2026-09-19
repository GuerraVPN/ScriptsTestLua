/* MIT License - Copyright (c) 2010 Serge Zaitsev */
#ifndef JSMN_H
#define JSMN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { JSMN_UNDEFINED=0, JSMN_OBJECT=1<<0, JSMN_ARRAY=1<<1, JSMN_STRING=1<<2, JSMN_PRIMITIVE=1<<3 } jsmntype_t;
enum jsmnerr { JSMN_ERROR_NOMEM=-1, JSMN_ERROR_INVAL=-2, JSMN_ERROR_PART=-3 };
typedef struct jsmntok { jsmntype_t type; int start; int end; int size; } jsmntok_t;
typedef struct jsmn_parser { unsigned int pos; unsigned int toknext; int toksuper; } jsmn_parser;
static jsmntok_t* jsmn_alloc_token(jsmn_parser* p, jsmntok_t* t, size_t n){ if(p->toknext>=n)return NULL; jsmntok_t* x=&t[p->toknext++]; x->start=x->end=-1;x->size=0;return x;}
static void jsmn_fill_token(jsmntok_t* t,jsmntype_t type,int s,int e){t->type=type;t->start=s;t->end=e;t->size=0;}
static int jsmn_parse_primitive(jsmn_parser* p,const char* js,size_t len,jsmntok_t* ts,size_t n){int s=p->pos;for(;p->pos<len&&js[p->pos]!='\0';p->pos++){char c=js[p->pos];if(c=='\t'||c=='\r'||c=='\n'||c==' '||c==','||c==']'||c=='}')break;if((unsigned char)c<32){p->pos=s;return JSMN_ERROR_INVAL;}}jsmntok_t* t=jsmn_alloc_token(p,ts,n);if(!t){p->pos=s;return JSMN_ERROR_NOMEM;}jsmn_fill_token(t,JSMN_PRIMITIVE,s,p->pos);p->pos--;return 0;}
static int jsmn_parse_string(jsmn_parser* p,const char* js,size_t len,jsmntok_t* ts,size_t n){int s=p->pos;p->pos++;for(;p->pos<len;p->pos++){char c=js[p->pos];if(c=='"'){jsmntok_t* t=jsmn_alloc_token(p,ts,n);if(!t){p->pos=s;return JSMN_ERROR_NOMEM;}jsmn_fill_token(t,JSMN_STRING,s+1,p->pos);return 0;}if(c=='\\'&&p->pos+1<len){p->pos++;if(js[p->pos]=='u'){for(int i=0;i<4&&p->pos+1<len;i++)p->pos++;}}}p->pos=s;return JSMN_ERROR_PART;}
static void jsmn_init(jsmn_parser* p){p->pos=0;p->toknext=0;p->toksuper=-1;}
static int jsmn_parse(jsmn_parser* p,const char* js,size_t len,jsmntok_t* ts,unsigned int n){int r,i,count=p->toknext;jsmntok_t* tok;for(;p->pos<len&&js[p->pos]!='\0';p->pos++){char c=js[p->pos];switch(c){case '{':case '[':count++;tok=jsmn_alloc_token(p,ts,n);if(!tok)return JSMN_ERROR_NOMEM;if(p->toksuper!=-1)ts[p->toksuper].size++;tok->type=(c=='{'?JSMN_OBJECT:JSMN_ARRAY);tok->start=p->pos;p->toksuper=p->toknext-1;break;case '}':case ']':{jsmntype_t type=(c=='}'?JSMN_OBJECT:JSMN_ARRAY);for(i=p->toknext-1;i>=0;i--){tok=&ts[i];if(tok->start!=-1&&tok->end==-1){if(tok->type!=type)return JSMN_ERROR_INVAL;tok->end=p->pos+1;p->toksuper=-1;break;}}if(i==-1)return JSMN_ERROR_INVAL;for(;i>=0;i--){tok=&ts[i];if(tok->start!=-1&&tok->end==-1){p->toksuper=i;break;}}break;}case '"':r=jsmn_parse_string(p,js,len,ts,n);if(r<0)return r;count++;if(p->toksuper!=-1)ts[p->toksuper].size++;break;case '\t':case '\r':case '\n':case ' ':break;case ':':p->toksuper=p->toknext-1;break;case ',':if(p->toksuper!=-1&&ts[p->toksuper].type!=JSMN_ARRAY&&ts[p->toksuper].type!=JSMN_OBJECT){for(i=p->toknext-1;i>=0;i--){if((ts[i].type==JSMN_ARRAY||ts[i].type==JSMN_OBJECT)&&ts[i].start!=-1&&ts[i].end==-1){p->toksuper=i;break;}}}break;default:r=jsmn_parse_primitive(p,js,len,ts,n);if(r<0)return r;count++;if(p->toksuper!=-1)ts[p->toksuper].size++;break;}}for(i=p->toknext-1;i>=0;i--){if(ts[i].start!=-1&&ts[i].end==-1)return JSMN_ERROR_PART;}return count;}
#ifdef __cplusplus
}
#endif
#endif
