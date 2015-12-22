#include <string>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <map>
#include <stdint.h>

#include "lm.h"
using namespace lm;
using namespace std;


//int register_node(const std::string& path, const CTX* ctx);

class Testbed
{
public:
  Testbed(int (*test)(void))
{
    m_test=test;
}
  Testbed(int (*test)(void),string name,string description)
  {
    Test& t=g_tests[test];
    t.m_name=name;
    t.m_description=description;
    t.result=1;
  }
  Testbed& operator, (int (*dependency)(void))
  {
    std::map<int (*)(void), Test>::iterator it;
    it=g_tests.find(m_test);
    if(it!=g_tests.end())
      {
        (*it).second.m_deps.push_back(dependency);
      }
    return *this;
  }
  static int run(int (*test)(void), bool forced)
  {
    std::map<int (*)(void), Test>::iterator it;
    it=g_tests.find(test);
    if(it!=g_tests.end())
      {
        //test found
        if((*it).second.result<1)
          {
            //printf("test already run\n");
            if(!forced) return 0;
            //return 0;
          }
        if((*it).second.result==2)
          {
            printf("circular dependency, dropping\n");
          }
        (*it).second.result=2;
        size_t i;
        for(i=0;i<(*it).second.m_deps.size();i++)
          {
            run((*it).second.m_deps[i],false);
          }
        (*it).second.result=0;
        printf("running test %s (%s)\n",(*it).second.m_name.c_str(),(*it).second.m_description.c_str());
        int result;
        result=(*it).first();
        printf("result=%d\n",result);
      }
    else
      {
        printf("no such test\n");
      }
    return 0;
  }
private:
  int (*m_test)(void); //used to define dependencies for operator,

  struct Test
  {
    //int (*m_test)(void);
    std::vector<int (*)(void)> m_deps;
    string m_name;
    string m_description;
    int result; //-1 test failed, 0 test passed, 1 test not run, 2 test about to be run (for dependencied)
  };
  static std::map<int (*)(void), Test> g_tests;
};
std::map<int (*)(void), Testbed::Test> Testbed::g_tests;


#define MAKEDEP(test_name,...) Testbed makedep_##test_name=(*new Testbed(test_name), __VA_ARGS__)
#define DEFTEST(test_name,test_description) int test_name(); Testbed deftest_##test_name=(*new Testbed(test_name,#test_name,test_description))
#define RUNTEST(test_name) Testbed::run(test_name,true)



result_e simplest_read(CTX* ctx, string& value)
{
  value="aaaa";
  return Success;
}

DEFTEST(test_register_unregister,"simplest register_node()/unregister_node()");
int test_register_unregister()
{
  CTX ctx={0};
  ctx.read=simplest_read;
  int result;
  result=register_node("aaaa",&ctx);
  if(result!=Success) return -1;
  result=unregister_node("aaaa");
  if(result!=Success) return -2;
  return 0;
}

DEFTEST(test_unregister_nonexistent,"unregister_node() that is not registered");
//MAKEDEP(test_unregister_nonexistent,test_register_unregister);
int test_unregister_nonexistent()
{
  CTX ctx={0};
  ctx.read=simplest_read;
  int result=0;
  if(register_node("a/b",&ctx)!=0) if(result==0) (result=-1);
  if(register_node("a/c",&ctx)!=0) if(result==0) (result=-2);
  if(register_node("b/d",&ctx)!=0) if(result==0) (result=-3);
  if(register_node("c/e",&ctx)!=0) if(result==0) (result=-4);

  if(unregister_node("a/b")!=0) if(result==0) result=-5;
  //   if(unregister_node("a/b")==0) if(result==0) result=-6;
  //   if(unregister_node("a/x")==0) if(result==0) result=-7;
  //   if(unregister_node("b")==0) if(result==0) result=-8;
  if(unregister_node("/a/c")!=0) if(result==0) result=-10;
  if(unregister_node("/b/d")!=0) if(result==0) result=-11;
  if(unregister_node("/c/e")!=0) if(result==0) result=-12;
  //if(unregister_node("/c/e")==0) if(result==0) result=-13;
  return result;
}


result_e echo_instance_read(CTX* ctx, string& value)
{
  char s[10];
  sprintf(s,"%d",(int)(uintptr_t)ctx->common);
  value=s;
  return Success;
}

DEFTEST(test_massive_register_unregister,"massive test of register_node()/unregister_node()");
MAKEDEP(test_massive_register_unregister,test_unregister_nonexistent);
int test_massive_register_unregister()
{
  CTX ctx={0};
  ctx.read=echo_instance_read;
  int result=0;
  int i,j;
  for(j=0;j<2*3*5*7*11*13;j++)
    {
      i=j*17;
      char s[30];
      sprintf(s,"%d/%d/%d/%d/%d/%d",i%2,i%3,i%5,i%7,i%11,i%13);
      ctx.common=(void*)(uintptr_t)(i%(2*3*5*7*11*13));
      if(register_node(s,&ctx)!=0)
        {
          result=-1000000-i;
          break;
        }
    }
  if(result==0)
    for(i=0;i<2*3*5*7*11*13;i++)
      {
        char s[30];
        sprintf(s,"%d/%d/%d/%d/%d/%d",i%2,i%3,i%5,i%7,i%11,i%13);
        Peek* p;
        string v;
        p=Peek::open(s);
        if(p==NULL) { result=-2000000-i; break; }
        if(p->read(v)!=0) { result=-3000000-i; break; }
        p->close();
        if(atoi(v.c_str())!=i) { result=-4000000-i; break; }
      }
  for(i=0;i<2*3*5*7*11*13;i++)
    {
      char s[30];
      sprintf(s,"%d/%d/%d/%d/%d/%d",i%2,i%3,i%5,i%7,i%11,i%13);
      if(unregister_node(s)!=0) if(result==0) result=-5000000-i; 
    }
  return result;
}


DEFTEST(test_Peek_open,"Peek::open on existing simple file");
MAKEDEP(test_Peek_open,test_register_unregister);
int test_Peek_open() 
{
  int result=0;
  CTX ctx={0};
  ctx.read=simplest_read;
  register_node("aaaa",&ctx);
  Peek* p;
  p=Peek::open("aaaa");
  if(p!=NULL)
    p->close();
  else
    result=-1;
  unregister_node("aaaa");
  return result;
}

DEFTEST(test_list_root,"Peek::list on '/'");
MAKEDEP(test_list_root,test_register_unregister);
int test_list_root() 
{
  int result=0;
  CTX ctx={0};
  ctx.read=simplest_read;
  register_node("aaaa",&ctx);
  Peek* p;
  p=Peek::open("/");
  if(p!=NULL)
    {
      std::vector< dir_item_d > files;
      if(p->list(files)==0)
        {
          if(files.size()==1)
            {
              if(files[0].name!="aaaa")
                result=-4;
            }
          else
            result=-3;
        }
      else
        result=-2;
      p->close();
    }
  else
    result=-1;
  unregister_node("aaaa");
  return result;
}


DEFTEST(test_simple_read,"Peek::read on simple node");
MAKEDEP(test_simple_read,test_Peek_open,test_register_unregister);
int test_simple_read() 
{
  int result=0;
  CTX ctx={0};
  ctx.read=simplest_read;
  register_node("aaaa",&ctx);
  Peek* p;
  p=Peek::open("aaaa");
  if(p!=NULL)
    {
      string v;
      if(p->read(v)==0)
        {
          if(v!="aaaa")
            result=-3;
          //success
        }
      else
        result=-2;
      p->close();
    }
  else
    result=-1;
  unregister_node("aaaa");
  return result;
}


int test_gfh_error=0;

result_e lm_from_handler_read(CTX* ctx, string& value)
{
  uintptr_t cnt=(uintptr_t)(void*)ctx->common;
  cnt++;
  char s[10];
  sprintf(s,"%lld",(long long int)cnt);
  value=s;
  if(cnt<20)
    {
      CTX ctx={0};
      ctx.read=lm_from_handler_read;
      ctx.common=(void*)cnt;
      if(register_node(s,&ctx)==0)
        {
          Peek* p=Peek::open(s);
          if(p!=NULL)
            {
              string v;
              if(p->read(v)!=0)
                test_gfh_error=-100*cnt-3;
              value+=v;
              p->close();
              unregister_node(s);
            }
          else
            test_gfh_error=-100*cnt-2;
        }
      else
        test_gfh_error=-100*cnt-1;
    }
  return Success;
}

DEFTEST(test_lm_recursion,"recursion of LM operations from Peek::read");
MAKEDEP(test_lm_recursion, test_simple_read,test_Peek_open,test_register_unregister);
int test_lm_recursion()
{
  int result=0;
  CTX ctx={0};
  ctx.read=lm_from_handler_read;
  ctx.common=(void*)0;
  register_node("0",&ctx);
  test_gfh_error=0;
  Peek* p;
  p=Peek::open("0");
  if(p!=NULL)
    {
      string v;
      if(p->read(v)==0)
        {
          if(test_gfh_error==0)
            {
              if(v!="1234567891011121314151617181920")
                result=-3;
            }
          else
            result=test_gfh_error;
          //success
        }
      else
        result=-2;
      p->close();
    }
  else
    result=-1;
  unregister_node("0");
  return result;
}




void cpl_r_action(int i);

result_e cpl_r_open(CTX* ctx,const std::string& path)            {cpl_r_action((uintptr_t)ctx->common);return Success;}
result_e cpl_r_read(CTX* ctx, std::string& value)                {cpl_r_action((uintptr_t)ctx->common);return Success;}
result_e cpl_r_write(CTX* ctx, const std::string& value)         {cpl_r_action((uintptr_t)ctx->common);return Success;}
result_e cpl_r_dirlist(CTX* ctx, std::vector<dir_item_d>& files) {cpl_r_action((uintptr_t)ctx->common);return Success;}
void     cpl_r_close(CTX* ctx)                                   {cpl_r_action((uintptr_t)ctx->common);}

int cpl_r_counter=0;
void cpl_r_action(int i)
{
  cpl_r_counter++;
  if(i==0) return;
  char name[10];
  sprintf(name,"%d",i);
  CTX ctx={0};
  ctx.open=cpl_r_open;
  ctx.read=cpl_r_read;
  ctx.write=cpl_r_write;
  ctx.dirlist=cpl_r_dirlist;
  ctx.close=cpl_r_close;
  ctx.common=(void*)(uintptr_t)(i-1);
  register_node(name,&ctx);
  Peek* p=Peek::open(name);
  if(p!=NULL)
    {
      string v;
      p->read(v);
      p->write(v);
      std::vector< dir_item_d > files;
      p->list(files);
      p->close();
    }
  unregister_node(name);
}

DEFTEST(test_complete_recursion_from_lm_handlers,"recursion of LM operations from Peek::read");
MAKEDEP(test_complete_recursion_from_lm_handlers,
        test_simple_read,test_Peek_open,test_register_unregister);
int test_complete_recursion_from_lm_handlers()
{
  cpl_r_counter=0;
  cpl_r_action(0);
  if(cpl_r_counter!=1) return -1;

  cpl_r_counter=0;
  cpl_r_action(1);
  if(cpl_r_counter!=1+5) return -2;

  cpl_r_counter=0;
  cpl_r_action(2);
  if(cpl_r_counter!=1+5+25) return -3;
  return 0;
}

result_e peekf_open(CTX* ctx,const std::string& path)            {return Success;}
result_e peekf_read(CTX* ctx, std::string& value)                {return Success;}
result_e peekf_write(CTX* ctx, const std::string& value)         {return Success;}
result_e peekf_dirlist(CTX* ctx, std::vector<dir_item_d>& files) {return Success;}
void     peekf_close(CTX* ctx) {}

int peekf_check(const char* name,unsigned int flags)
{
  int result=-1;
  Peek* p;
  p=Peek::open(name);
  if(p!=NULL)
    {
      if(p->flags()==flags) result=0;
      p->close();
    }
  unregister_node(name);
  return result;
}
DEFTEST(test_Peek_flags,"check Peek::flags");
MAKEDEP(test_Peek_flags,test_Peek_open,test_register_unregister);
int test_Peek_flags() 
{
  CTX ctx={0};
  register_node("0",&ctx);

  ctx.read=peekf_read;
  register_node("r",&ctx);

  ctx.write=peekf_write;
  register_node("rw",&ctx);

  ctx.read=NULL;
  register_node("w",&ctx);

  ctx.dirlist=peekf_dirlist;
  register_node("dw",&ctx);

  ctx.read=peekf_read;
  register_node("drw",&ctx);

  ctx.write=NULL;
  register_node("dr",&ctx);

  ctx.read=NULL;
  register_node("d",&ctx);

  int result=0;
  if(peekf_check("0",0)!=0)                    if(result==0) result=-1;
  if(peekf_check("r",F_RD)!=0)                 if(result==0) result=-2;
  if(peekf_check("w",F_WR)!=0)                 if(result==0) result=-3;
  if(peekf_check("rw",F_RD|F_WR)!=0)           if(result==0) result=-4;
  if(peekf_check("d",F_DIR)!=0)                if(result==0) result=-5;
  if(peekf_check("dr",F_DIR|F_RD)!=0)          if(result==0) result=-6;
  if(peekf_check("dw",F_DIR|F_WR)!=0)          if(result==0) result=-7;
  if(peekf_check("drw",F_RD|F_WR|F_DIR)!=0)    if(result==0) result=-8;
  return result;
}

DEFTEST(test_using_unregistered_node,"using open nodes after unregister");
MAKEDEP(test_using_unregistered_node,test_Peek_flags,test_Peek_open,test_register_unregister);
int test_using_unregistered_node() 
{
  CTX ctx={0};
  int result=0;
  ctx.open=peekf_open;
  ctx.read=peekf_read;
  ctx.write=peekf_write;
  ctx.dirlist=peekf_dirlist;
  ctx.close=peekf_close;
  register_node("aaaa",&ctx);
  Peek* p=Peek::open("aaaa");
  unregister_node("aaaa");
  if(p!=NULL)
    {
      string v;
      std::vector< dir_item_d > files;
      if(p->read(v)!=NodeAlreadyClosed) result=-2;
      if(p->write(v)!=NodeAlreadyClosed) if(result==0) result=-2;
      if(p->list(files)!=NodeAlreadyClosed) if(result==0) result=-3;
      p->close();
    }
  else
    result=-1;

  return result;
}

sem_t unregister_in_api_sem;
volatile int unregister_in_api_marker;
void unregister_in_api_action()
{
  sem_post(&unregister_in_api_sem);
  unregister_in_api_marker=0;
  usleep(300*1000);
  unregister_in_api_marker=2;
}
void* unregister_in_api_helper(void* arg)
{
  sem_wait(&unregister_in_api_sem);
  unregister_node("aaaa");
  unregister_in_api_marker=1;
  return NULL;
}
result_e in_api_open(CTX* ctx,const std::string& path)            {unregister_in_api_action();return Success;}
result_e in_api_read(CTX* ctx, std::string& value)                {unregister_in_api_action();return Success;}
result_e in_api_write(CTX* ctx, const std::string& value)         {unregister_in_api_action();return Success;}
result_e in_api_dirlist(CTX* ctx, std::vector<dir_item_d>& files)    {unregister_in_api_action();return Success;}
void    in_api_close(CTX* ctx)                                   {unregister_in_api_action();}
result_e in_api_open_x(CTX* ctx,const std::string& path)          {return Success;}
void    in_api_close_x(CTX* ctx)                                 {}

DEFTEST(test_unregistering_in_api,"unregistering while in Peek::*");
//MAKEDEP(test_unregistering_in_api,test_Peek_flags,test_Peek_open,test_register_unregister);
int test_unregistering_in_api() 
{
  CTX ctx={0};
  int result=0;
  ctx.open=in_api_open;
  ctx.read=in_api_read;
  ctx.write=in_api_write;
  ctx.dirlist=in_api_dirlist;
  ctx.close=in_api_close_x;
  sem_init(&unregister_in_api_sem,0,0);
  pthread_t t1;

  int x;
  x=register_node("aaaa",&ctx);
  if(x!=0) return -1;
  pthread_create(&t1,NULL,unregister_in_api_helper,NULL);
  Peek* p=Peek::open("aaaa");
  p->close();
  pthread_join(t1,NULL);
  if(unregister_in_api_marker!=1) return -2;
  return result;
}



int test_0_b()
{
  Peek* p;
  p=Peek::open("aaaa/bbb");
  if(p!=NULL)
    {
      p->close();
      return -1;
    }
  return 0;
}
int test_0_c()
{
  int r;
  r=unregister_node("aaaa1");
  if(r==0) return -1;
  r=unregister_node("aaaa");
  if(r!=0) return -2;
  return 0;
}


result_e test_1_open(CTX* ctx, const string& path)
{
  string* s;
  s=new string();
  (*s)=path+path;
  ctx->instance=(void*)s;
  return Success;
}
result_e test_1_read(CTX* ctx, string& value)
{
  value=*(string*)(ctx->instance);
  return Success;
}
void test_1_close(CTX* ctx)
{
  delete (string*)(ctx->instance);
}
int test_1()
{
  CTX ctx={0};
  ctx.open=test_1_open;
  ctx.read=test_1_read;
  ctx.close=test_1_close;

  int r;
  r=register_node("pot",&ctx);
  if(r!=0) return -1;
  Peek* p;
  p=Peek::open("pot/flowers");
  if(p==NULL) return -2;
  string s;
  r=p->read(s);
  if(r!=0) return -3;
  if(s!="flowersflowers") return -4;
  p->close();
  r=unregister_node("pot");
  if(r!=0) return -5;

  return 0;
}

int test_2()
{
  Peek* p;
  p=Peek::open("/");
  if(p==NULL) return -1;
  std::vector< dir_item_d > dir;
  int r;
  r=p->list(dir);
  if(r!=0) return -2;
  if(dir.size()!=0) return -3;
  //printf("%s\n",dir[0].name.c_str());
  return 0;
}




result_e test_auto_dirs_read(CTX* ctx, string& value)
{
  value="aaaa";
  return Success;
}
int test_auto_dirs()
{
  CTX ctx={0};
  ctx.read=test_auto_dirs_read;
  int r;
  r=register_node("pot",&ctx);
  if(r!=0) return -1;
  r=register_node("bucket",&ctx);
  if(r!=0) return -2;
  r=register_node("glass",&ctx);
  if(r!=0) return -3;
  r=register_node("tree/fir",&ctx);
  if(r!=0) return -4;
#if 1
  r=register_node("tree/birch",&ctx);
  if(r!=0) return -5;
  r=register_node("tree/oak",&ctx);
  if(r!=0) return -6;
#endif
  Peek* p;
  p=Peek::open("/");
  if(p==NULL) return -7;
  std::vector< dir_item_d > dir;
  r=p->list(dir);
  if(r!=0) return -8;
  if(dir.size()!=4) return -9;
  if(dir[0].name!="bucket") return -10;
  if(dir[1].name!="glass") return -11;
  if(dir[2].name!="pot") return -19;
  if(dir[3].name!="tree") return -12;
  p->close();
  p=Peek::open("/tree/");
  if(p==NULL) return -13;
  r=p->list(dir);
  if(r!=0) return -14;
  if(dir.size()!=3) return -15;
  if(dir[0].name!="birch") return -16;
  if(dir[1].name!="fir") return -17;
  if(dir[2].name!="oak") return -18;
  r=unregister_node("pot");
  if(r!=0) return -19;
  r=unregister_node("bucket");
  if(r!=0) return -20;
  r=unregister_node("glass");
  if(r!=0) return -21;
  r=unregister_node("tree/fir");
  if(r!=0) return -22;
  r=unregister_node("tree/birch");
  if(r!=0) return -23;
  r=unregister_node("tree/oak");
  if(r!=0) return -24;
  return 0;
}



result_e test_nameecho_open(CTX* ctx, const string& path)
{
  string* s;
  s=new string();
  (*s)=path;
  ctx->instance=(void*)s;
  return Success;
}
result_e test_nameecho_read(CTX* ctx, string& value)
{
  result_e result=Success;
  string* s=(string*)(ctx->instance);
  if(*s=="")
    {
      result=MoreData;
      value="";
    }
  else
    {
      value=(*s)[0];
      *s=s->substr(1);
    }
  return result;
}
void test_nameecho_close(CTX* ctx)
{
  delete (string*)(ctx->instance);
}


int test_multiread()
{
  CTX ctx={test_nameecho_open,test_nameecho_read,NULL,NULL,test_nameecho_close,NULL,NULL};
  int r,i,j;
  r=register_node("garden",&ctx);
  if(r!=0) return -1;

  const char* x[8]={"river","lilly","raspberry","grass","pond","pond/frog","pond/fish","pond/frog/tadpole"};
  Peek* p[8];
  for(i=0;i<8;i++)
    {
      p[i]=Peek::open(string("garden/")+x[i]);
      if(p[i]==NULL) return -10-i;
    }
  string result[8];
  for(j=0;j<25;j++)
    {
      for(i=0;i<8;i++)
        {
          string v;
          r=p[i]->read(v);
          result[i]+=v;
        }
    }
  for(i=0;i<8;i++)
    if(result[i]!=x[i])
      return -20-i;
  for(i=0;i<8;i++)
    p[i]->close();

  r=unregister_node("garden");
  if(r!=0) return -30;
  return 0;
}




result_e test_nameecho_read_sleep(CTX* ctx, string& value)
{
  result_e result=Success;
  string* s=(string*)(ctx->instance);
  value=*s;
  usleep(300*1000);
  return result;
}
void* test_unregister_wait_1(void* arg)
{
  Peek* p=Peek::open("garden/butterfly");
  string s;
  p->read(s);
  return NULL;
}
void* test_unregister_wait_2(void* arg)
{
  int* res=(int*)arg;
  usleep(200*1000);
  *res=unregister_node("garden");
  return NULL;
}
int test_unregister_wait()
{
  CTX ctx={test_nameecho_open,test_nameecho_read_sleep,NULL,NULL,test_nameecho_close,NULL,NULL};
  int r;
  r=register_node("garden",&ctx);
  if(r!=0) return -1;
  pthread_t t1,t2;
  int res1;
  pthread_create(&t1,NULL,test_unregister_wait_1,NULL);
  pthread_create(&t2,NULL,test_unregister_wait_2,&res1);
  usleep(100*1000);
  r=unregister_node("garden");
  if(r!=0) return -2;
  if(res1!=-1) return -3;
  return 0;
}








void test_forced_close_close(CTX* ctx)
{
  __sync_fetch_and_add((int*)ctx->common,1);
  //printf("closing\n");
}
int test_forced_close()
{
  volatile int cnt=0;
  CTX ctx={NULL,NULL,NULL,NULL,test_forced_close_close,(void*)&cnt,NULL};
  int r;
  r=register_node("garden",&ctx);
  Peek* p1=Peek::open("garden");
  Peek* p2=Peek::open("garden");
  r=unregister_node("garden");
  if(r!=0) return -1;
  if(cnt!=2) return -2;
  p1->close();
  p2->close();
  return 0;
}





void execute_test(int (*x)(),const char* name)
{
  printf("executing %s",name);
  int res=x();
  if(res==0)
    printf(" OK\n");
  else
    printf(" result=%d\n",res);
}

#define DOTEST(x) execute_test(x,#x)

void* one_iteration(void* arg)
{
  RUNTEST(test_Peek_open);
  RUNTEST(test_register_unregister);


  RUNTEST(test_list_root);
  RUNTEST(test_unregister_nonexistent);

  RUNTEST(test_massive_register_unregister);

  RUNTEST(test_lm_recursion);
  RUNTEST(test_complete_recursion_from_lm_handlers);
  RUNTEST(test_Peek_flags);

  RUNTEST(test_using_unregistered_node);
  RUNTEST(test_unregistering_in_api);
  return NULL;
}

int main(int argc, char** argv)
{
  int i;
  for(i=0;i<1;i++)
    {
      one_iteration(NULL);
      usleep(100000);
    }
  for(i=0;i<000000;i++)
    {
      pthread_t t1;

      pthread_create(&t1,NULL,one_iteration,NULL);
      pthread_join(t1,NULL);
      printf("sss\n");
      //sleep(5);
    }

}
