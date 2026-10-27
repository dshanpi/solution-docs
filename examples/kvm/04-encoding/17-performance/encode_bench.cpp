#include "sequence.h"
#include "AWVideoEncoder.h"
#include "sunxiMemInterface.h"
#include "sdklog.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <time.h>
#include <signal.h>
#include <unistd.h>
using namespace awvideoencoder;
static volatile sig_atomic_t running=1;
static void stop(int){running=0;}
static int64_t ns(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return int64_t(t.tv_sec)*1000000000+t.tv_nsec;}
class Output:public AWVideoEncoderDataCallback {
public:
 FILE*file;uint64_t bytes=0;int frames=0;bool failed=false;
 explicit Output(FILE*f):file(f){}
 int encoderDataReady(AVPacket*p)override{
  if(!p)return -1;
  if(p->dataLen0 && fwrite(p->pAddrVir0,1,p->dataLen0,file)!=p->dataLen0)failed=true;
  if(p->dataLen1 && fwrite(p->pAddrVir1,1,p->dataLen1,file)!=p->dataLen1)failed=true;
  bytes+=p->dataLen0+p->dataLen1;if(p->pts!=-1)frames++;
  return failed?-1:0;
 }
};
int main(int argc,char**argv){
 if(argc!=4)return 2;
 signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGALRM,stop);alarm(20);sdk_log_setlevel(2);
 int kbps=atoi(argv[2]);if(kbps<1000||kbps>30000)return 3;
 FILE*f=fopen(argv[1],"rb");if(!f)return 4;
 std::vector<uint8_t> base(BYTES);
 if(fread(base.data(),1,BYTES,f)!=BYTES)return 5;fclose(f);
 FILE*output=fopen(argv[3],"wb");if(!output)return 6;
 setvbuf(output,nullptr,_IOFBF,1024*1024);Output cb(output);
 fprintf(stderr,"stage allocOpen\n");dma_mem_des_t mem={};if(allocOpen(MEM_TYPE_CDX_NEW,&mem,nullptr)<0)return 7;
 fprintf(stderr,"stage allocAlloc\n");mem.size=W*((H+15)&~15)*3/2;
 if(allocAlloc(MEM_TYPE_CDX_NEW,&mem,nullptr)<0){allocClose(MEM_TYPE_CDX_NEW,&mem,nullptr);return 8;}
 fprintf(stderr,"stage params vir=%p phy=%lx\n",mem.vir,mem.phy);EncodeParam param={};param.codecType=CODEC_H264;param.pixelFormat=PIXEL_YUV420SP;
 param.srcW=param.dstW=W;param.srcH=param.dstH=H;param.rotation=Angle_0;
 param.bitRate=kbps*1000;param.frameRate=60;param.maxKeyFrame=60;
 param.frameCount=MULTI_FRAMES;param.rcMode=VBR;param.minQp=20;param.maxQp=28;
 fprintf(stderr,"stage encoder init\n");AWVideoEncoder* enc=AWVideoEncoder::create();int status=enc?enc->init(&param,&cb):-1;
 fprintf(stderr,"stage encode status=%d\n",status);std::vector<double> elapsed;int errors=0,over=0;int64_t start=ns();
 if(status==0)for(int i=0;i<N&&running;i++){
  int64_t target=start+int64_t(i)*1000000000/60;timespec wait={target/1000000000,target%1000000000};
  clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&wait,nullptr);
  make_frame((uint8_t*)mem.vir,base.data(),i);flushCache(MEM_TYPE_CDX_NEW,&mem,nullptr);
  AVPacket packet={};packet.id=i;packet.pts=int64_t(i)*1000/60;packet.isKeyframe=i==0;
  packet.pAddrPhy0=(unsigned char*)(uintptr_t)mem.phy;packet.pAddrPhy1=packet.pAddrPhy0+W*H;
  packet.dataLen0=W*H;packet.dataLen1=W*H/2;
  int64_t t=ns();int ret=enc->encode(&packet);double ms=(ns()-t)/1000000.0;
  elapsed.push_back(ms);if(ms>1000.0/60)over++;if(ret<0)errors++;
 }
 fprintf(stderr,"stage destroy frames=%d\n",cb.frames);if(enc)AWVideoEncoder::destroy(enc);
 fprintf(stderr,"stage free ion\n");allocFree(MEM_TYPE_CDX_NEW,&mem,nullptr);allocClose(MEM_TYPE_CDX_NEW,&mem,nullptr);
 if(fclose(output)!=0)cb.failed=true;
 double sum=0;for(double v:elapsed)sum+=v;std::sort(elapsed.begin(),elapsed.end());
 auto percentile=[&](double q){return elapsed.empty()?0:elapsed[std::min(elapsed.size()-1,size_t(q*(elapsed.size()-1)))];};
 printf("RESULT {\"kbps\":%d,\"frames\":%d,\"bytes\":%llu,\"actual_mbps\":%.6f,\"encode_avg_ms\":%.4f,\"encode_p95_ms\":%.4f,\"encode_p99_ms\":%.4f,\"encode_max_ms\":%.4f,\"over_16_67ms\":%d,\"errors\":%d}\n",kbps,cb.frames,(unsigned long long)cb.bytes,cb.bytes*8.0/(N/60.0)/1000000,elapsed.empty()?0:sum/elapsed.size(),percentile(.95),percentile(.99),percentile(1),over,errors);
 return status==0&&cb.frames==N&&errors==0&&!cb.failed?0:9;
}
