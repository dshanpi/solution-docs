#include "sequence.h"
#include <cstdio>
#include <vector>
int main(int argc,char**argv){
 if(argc!=2)return 2;
 FILE*f=fopen(argv[1],"rb");if(!f)return 3;
 std::vector<uint8_t> base(BYTES),frame(BYTES);
 if(fread(base.data(),1,BYTES,f)!=BYTES)return 4;fclose(f);
 for(int i=0;i<N;i++){make_frame(frame.data(),base.data(),i);if(fwrite(frame.data(),1,BYTES,stdout)!=BYTES)return 5;}
 return 0;
}
