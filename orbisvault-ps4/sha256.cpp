#include "sha256.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ov {

namespace {

struct Sha256Ctx {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
};

constexpr uint32_t K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

inline uint32_t rotr(uint32_t a, uint32_t b) { return (a >> b) | (a << (32-b)); }
inline uint32_t ch(uint32_t x,uint32_t y,uint32_t z){return (x&y)^(~x&z);}
inline uint32_t maj(uint32_t x,uint32_t y,uint32_t z){return (x&y)^(x&z)^(y&z);}
inline uint32_t ep0(uint32_t x){return rotr(x,2)^rotr(x,13)^rotr(x,22);}
inline uint32_t ep1(uint32_t x){return rotr(x,6)^rotr(x,11)^rotr(x,25);}
inline uint32_t sig0(uint32_t x){return rotr(x,7)^rotr(x,18)^(x>>3);}
inline uint32_t sig1(uint32_t x){return rotr(x,17)^rotr(x,19)^(x>>10);}

void transform(Sha256Ctx& c,const uint8_t data[64]){
    uint32_t m[64];
    for(int i=0,j=0;i<16;i++,j+=4)
        m[i]=(uint32_t(data[j])<<24)|(uint32_t(data[j+1])<<16)|(uint32_t(data[j+2])<<8)|data[j+3];
    for(int i=16;i<64;i++) m[i]=sig1(m[i-2])+m[i-7]+sig0(m[i-15])+m[i-16];

    uint32_t a=c.state[0],b=c.state[1],cc=c.state[2],d=c.state[3];
    uint32_t e=c.state[4],f=c.state[5],g=c.state[6],h=c.state[7];
    for(int i=0;i<64;i++){
        uint32_t t1=h+ep1(e)+ch(e,f,g)+K[i]+m[i];
        uint32_t t2=ep0(a)+maj(a,b,cc);
        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c.state[0]+=a;c.state[1]+=b;c.state[2]+=cc;c.state[3]+=d;
    c.state[4]+=e;c.state[5]+=f;c.state[6]+=g;c.state[7]+=h;
}

void init(Sha256Ctx& c){
    c.datalen=0;c.bitlen=0;
    c.state[0]=0x6a09e667;c.state[1]=0xbb67ae85;c.state[2]=0x3c6ef372;c.state[3]=0xa54ff53a;
    c.state[4]=0x510e527f;c.state[5]=0x9b05688c;c.state[6]=0x1f83d9ab;c.state[7]=0x5be0cd19;
}

void update(Sha256Ctx& c,const uint8_t* data,size_t len){
    for(size_t i=0;i<len;i++){
        c.data[c.datalen++]=data[i];
        if(c.datalen==64){
            transform(c,c.data);
            c.bitlen+=512;
            c.datalen=0;
        }
    }
}

void final(Sha256Ctx& c,uint8_t hash[32]){
    uint32_t i=c.datalen;
    if(c.datalen<56){
        c.data[i++]=0x80;
        while(i<56)c.data[i++]=0;
    }else{
        c.data[i++]=0x80;
        while(i<64)c.data[i++]=0;
        transform(c,c.data);
        memset(c.data,0,56);
    }

    c.bitlen += uint64_t(c.datalen)*8;
    c.data[63]=c.bitlen;
    c.data[62]=c.bitlen>>8;
    c.data[61]=c.bitlen>>16;
    c.data[60]=c.bitlen>>24;
    c.data[59]=c.bitlen>>32;
    c.data[58]=c.bitlen>>40;
    c.data[57]=c.bitlen>>48;
    c.data[56]=c.bitlen>>56;
    transform(c,c.data);

    for(i=0;i<4;i++){
        hash[i]=(c.state[0]>>(24-i*8))&0xff;
        hash[i+4]=(c.state[1]>>(24-i*8))&0xff;
        hash[i+8]=(c.state[2]>>(24-i*8))&0xff;
        hash[i+12]=(c.state[3]>>(24-i*8))&0xff;
        hash[i+16]=(c.state[4]>>(24-i*8))&0xff;
        hash[i+20]=(c.state[5]>>(24-i*8))&0xff;
        hash[i+24]=(c.state[6]>>(24-i*8))&0xff;
        hash[i+28]=(c.state[7]>>(24-i*8))&0xff;
    }
}

} // namespace

std::string sha256File(const std::string& path){
    FILE* f=fopen(path.c_str(),"rb");
    if(!f)return "";

    Sha256Ctx ctx;
    init(ctx);
    uint8_t buf[64*1024];
    for(;;){
        size_t n=fread(buf,1,sizeof(buf),f);
        if(n)update(ctx,buf,n);
        if(n<sizeof(buf)){
            if(ferror(f)){fclose(f);return "";}
            break;
        }
    }
    fclose(f);

    uint8_t hash[32];
    final(ctx,hash);
    static const char* hex="0123456789abcdef";
    std::string out;
    out.resize(64);
    for(int i=0;i<32;i++){
        out[i*2]=hex[(hash[i]>>4)&0xf];
        out[i*2+1]=hex[hash[i]&0xf];
    }
    return out;
}

} // namespace ov
