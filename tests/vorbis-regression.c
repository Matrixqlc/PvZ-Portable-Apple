#define SDL_MAIN_HANDLED
#ifdef PVZ_TEST_SDL
#include <SDL.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#ifdef PVZ_TEST_SDL
#define STB_VORBIS_NO_STDIO 1
#define STB_VORBIS_NO_PUSHDATA_API 1
#define STB_VORBIS_MAX_CHANNELS 32
#define STB_VORBIS_SDL 1
#endif
#include PVZ_VORBIS_HEADER
static void write_item(FILE *out,const void *p,size_t n) {assert(fwrite(p,1,n,out)==n);}
static void boundaries(void) {
 unsigned char storage[64]={0}; stb_vorbis f; ProbedPage page;
 vorbis_init(&f,NULL);f.stream_start=storage;f.stream_end=storage+sizeof(storage);f.stream=storage;f.stream_len=sizeof(storage);
 assert(set_file_offset(&f,0)==1&&f.stream==storage);
 assert(set_file_offset(&f,63)==1&&f.stream==storage+63);
 assert(set_file_offset(&f,64)==0&&f.stream==storage+64&&f.eof);
 assert(set_file_offset(&f,UINT32_MAX)==0&&f.stream==storage+64&&f.eof);
 // A header claiming 255 payload bytes in a 28-byte stream must be rejected.
 memset(storage,0,sizeof(storage));memcpy(storage,"OggS",4);storage[26]=1;storage[27]=255;
 f.stream=storage;f.stream_end=storage+28;f.stream_len=28;f.eof=0;
 assert(get_seek_page_info(&f,&page)==0&&f.error==VORBIS_unexpected_eof);
 // The largest Ogg sequence number is unsigned, not a signed sample position.
 memset(storage,0,sizeof(storage));memset(storage+14,255,4);
 f.stream=storage;f.stream_end=storage+23;f.stream_len=23;f.first_decode=0;f.eof=0;
 assert(start_page_no_capturepattern(&f)==1&&f.last_page==UINT32_MAX);
 puts("BOUNDARIES_PASS");
}
int main(int argc,char **argv) {
 if(argc==2&&strcmp(argv[1],"--boundaries")==0) {boundaries();return 0;}
 assert(argc==5);
 FILE *in=fopen(argv[1],"rb");assert(in);assert(fseek(in,0,SEEK_END)==0);long size=ftell(in);assert(size>0&&size<INT32_MAX);rewind(in);
 unsigned char *data=malloc((size_t)size);assert(data);assert(fread(data,1,(size_t)size,in)==(size_t)size);fclose(in);
 int error=0;stb_vorbis *v;
 if(strcmp(argv[4],"memory")==0)v=stb_vorbis_open_memory(data,(int)size,&error,NULL);
 #ifndef PVZ_TEST_SDL
 else v=stb_vorbis_open_filename(argv[1],&error,NULL);
#else
 else v=stb_vorbis_open_rwops(SDL_RWFromConstMem(data,(int)size),1,&error,NULL);
#endif
 if(!v){fprintf(stderr,"open failed %d\n",error);return 2;}
 stb_vorbis_info info=stb_vorbis_get_info(v);assert(info.channels>0&&info.channels<=64);
 int64 length=stb_vorbis_stream_length_in_samples(v);
 FILE *pcm=fopen(argv[2],"wb");FILE *seeks=fopen(argv[3],"wb");assert(pcm&&seeks);
 short buffer[128*64];int count;int64 total=0;
 while((count=stb_vorbis_get_samples_short_interleaved(v,info.channels,buffer,128*info.channels))>0){write_item(pcm,buffer,(size_t)count*(size_t)info.channels*sizeof(short));total+=count;}
 int64 positions[]={0,1,length/2,length>0?length-1:0,length,length+1,INT64_C(4294967297),-1};
 for(size_t i=0;i<sizeof(positions)/sizeof(positions[0]);++i){
  (void)stb_vorbis_get_error(v);int ok=stb_vorbis_seek(v,positions[i]);int err=stb_vorbis_get_error(v);int n=0;
  if(ok)n=stb_vorbis_get_samples_short_interleaved(v,info.channels,buffer,128*info.channels);
  write_item(seeks,&positions[i],sizeof(positions[i]));write_item(seeks,&ok,sizeof(ok));write_item(seeks,&err,sizeof(err));write_item(seeks,&n,sizeof(n));if(n)write_item(seeks,buffer,(size_t)n*(size_t)info.channels*sizeof(short));
 }
 fclose(pcm);fclose(seeks);stb_vorbis_close(v);free(data);
 printf("{\"channels\":%d,\"rate\":%u,\"length\":%lld,\"decoded_frames\":%lld}\n",info.channels,info.sample_rate,(long long)length,(long long)total);
 return 0;
}
