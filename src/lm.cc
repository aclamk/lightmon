#include <string>
#include <vector>
#include <list>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <stdio.h>
#include <assert.h>
#include <semaphore.h>
#define COMPILING_LM 1
#include "lm.h"

/*
  Locking contracts.
  Operations related to tree structure are locked by structure_lock.
  These operations include:
  - register node
  - unregister node
  - open (create Peek)
  - close (delete Peek)

  Operations on Peek's API are locked by api_use_count atomic counter.
  Node cannot be unregistered when any api call is in progress.

  Each API use changes api_use_count by 4. This leaves place for 2 bits of signaling.
  When unregister is requested 0x1 is signalled.
  If any API is in progress api_
  
 */

using namespace std;
extern "C"
{
   int lm_register_node(const std::string& in_path, const lm::CTX* ctx);
   int lm_unregister_node(const std::string& in_path);
}

namespace lm
{
class Peek;
class Manager
   {
      friend class Peek;
      friend int ::lm_register_node(const std::string& in_path, const lm::CTX* ctx);
      friend int ::lm_unregister_node(const std::string& in_path);
      static Manager* instance;
      public:
      static Manager& getInstance()
      {
         if(instance==NULL)
            instance=new Manager();
         return *instance;
      }
      static void deleteInstance()
      {
         if(instance!=NULL)
            delete instance;
         instance=NULL;
      }
      Manager();
      
      private:
      class Entry;
      class EntryDir;
      class EntryNode;

      typedef std::map<std::string,Entry*> DirEntryMap;//dir_elems_t;
         
      static CTX DirListCTX;
      static result_e DirList_dirlist(CTX* ctx, std::vector<dir_item_d>& files);
      static void DirList_close(CTX* ctx);
         
         
      Entry* root;
      pthread_mutex_t structure_lock;

      EntryDir* create_dir_structure(const std::string& in_path);      
      Entry* find_entry(const std::string& in_path,std::string& remainder);
      
         
         //   node: /path
         //   open: /path/ /path

      
      private:
      int register_node_internal(const std::string& in_path, const CTX* ctx);
      int unregister_node_internal(const std::string& in_path);
      int register_node(const std::string& in_path,const CTX* ctx);
      int unregister_node(const std::string& in_path);
         
      Peek* do_open(const std::string& in_name);
      void do_close(Peek* p);
   };

   //Manager::pthread_mutex_t structure_lock=PTHREAD_MUTEX_INITIALIZER;


class Manager::Entry
{
   protected:
      Entry()
      {
         //printf("+++ %p\n",this);
         api_use_cnt=0;
         ref_cnt=1;
         sem_init(&api_cleared,0,0);
      }
   public:
      ~Entry()
         {
            //printf("--- %p\n",this);
         }
      enum t_type {Dir,Node} type;
      inline EntryNode* as_node()
      {
         assert(type==Node);
         return (EntryNode*)this;
      }
      inline EntryDir* as_dir()
      {
         assert(type==Dir);
         return (EntryDir*)this;         
      }

      inline void ref();
      /*
      {
         int cnt;
         cnt=__sync_add_and_fetch(&ref_cnt,1);
         printf("%d %p %d\n",type,this,cnt);
         assert(cnt>=2&&"ref-ing dangling Entry");
      }
      */
      inline void unref();
      /*
      {
         int cnt;
         cnt=__sync_sub_and_fetch(&ref_cnt,1);
         printf("%d %p %d\n",type,this,cnt);
         assert(cnt>=0&&"unref below 0 for Entry");
         if(cnt==0)
         {
            if(type==Node) delete as_node(); else delete as_dir();
         }
      }
      */
      int api_begin()
      {
         int cnt;
         cnt=__sync_add_and_fetch(&api_use_cnt,4);
         if(cnt&1) //pending_unregister
         {
            //undo
            api_end();
            return 0;
         }
         return 1;
      }
      void api_end()
      {
         int cnt;
         cnt=__sync_sub_and_fetch(&api_use_cnt,4);
         if(cnt==1) //every api just left and pending_unregister was signalled, no ack yet
         {
            cnt=__sync_fetch_and_or(&api_use_cnt,0x2);
            if(cnt&0x2)
            {
               //someone already signalled...
            }
            else
            {
               //clear to go
               sem_post(&api_cleared);
            }
         }
      }

      //must be called with structure_lock taken
      void add_peek(Peek* p)
      {
         peeks.push_back(p);
         ref();
      }
      //must be called with structure_lock taken
      void remove_peek(Peek* p)
      {
         peeks.remove(p);
         unref();
         //if(peeks.size()==0 && (api_use_cnt&0x2)==0x2 )
         //   delete this;
         
      }
      
      volatile int api_use_cnt; //count of use by Peeks' API      
      sem_t api_cleared;        //to acknowledge no one is in API
      volatile int ref_cnt;
      list<Peek*> peeks;        //list of Peeks relating to this Entry
    
};


      
//      typedef std::map<std::string,Entry*> dir_elems_t;
class Manager::EntryDir : public Manager::Entry
{
   public:            
      DirEntryMap dir_elems;
      EntryDir()
      {
         type=Dir;
      }

};
   
class Manager::EntryNode : public Manager::Entry
{
   public:
      CTX base_ctx;
      EntryNode()
      {
         type=Node;
      }      
};    
   
inline void Manager::Entry::ref()
{
   int cnt;
   cnt=__sync_add_and_fetch(&ref_cnt,1);
   //printf("%d %p %d\n",type,this,cnt);
   assert(cnt>=2&&"ref-ing dangling Entry");
}
inline void Manager::Entry::unref()
{
   int cnt;
   cnt=__sync_sub_and_fetch(&ref_cnt,1);
   //printf("%d %p %d\n",type,this,cnt);
   assert(cnt>=0&&"unref below 0 for Entry");
   if(cnt==0)
   {
      if(type==Node) delete as_node(); else delete as_dir();
   }
}

   
class Peek
{
   private:
      static Peek* open(const std::string& path);
      void close();
      int read(std::string& );
      int write(const std::string& );
      int list(std::vector< dir_item_d >&);
      unsigned int flags();
      Peek(CTX* base_ctx,Manager::Entry* base_entry);
      ~Peek();
      CTX ctx;
      Manager::Entry* entry;
      friend class Manager;
};    

      
   
Manager::Manager()
{
   //root=new Entry();
   root=new EntryDir();
   pthread_mutexattr_t attr; 
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(&structure_lock,&attr);
   //root->type=Entry::Dir;
}


result_e Manager::DirList_dirlist(CTX* ctx, std::vector<dir_item_d>& files)
{
   EntryDir* e=(EntryDir*)ctx->instance;
   assert(e->type==Entry::Dir);
   files.clear();
   DirEntryMap::iterator it;
   DirEntryMap* de=&e->dir_elems;
   for(it=de->begin();it!=de->end();it++)
   {
      dir_item_d di;
      di.name=it->first;
      unsigned int f;
      if(it->second->type==Entry::Dir)
	f=F_DIR;
      else
	{
	  CTX* x=&it->second->as_node()->base_ctx;
	  if(x->read) f=F_RD;
	  if(x->write) f|=F_WR;
	}
      di.flags=f;
      files.push_back(di);
   }
   return Success;
}
void Manager::DirList_close(CTX* ctx)
{
   //Entry* e=(Entry*)ctx->instance_ctx;
   //delete e;
}
         
         

Manager::EntryDir* Manager::create_dir_structure(const std::string& in_path)
{
   EntryDir* dir=root->as_dir();
   char* path=strdup(in_path.c_str());
   char* strtok_ctx;
   char* ptr;
   ptr=strtok_r(path,"/",&strtok_ctx);
   while(ptr!=NULL)
   {
      DirEntryMap::iterator it;
      it=dir->dir_elems.find(ptr);
      if(it==dir->dir_elems.end())
      {
         //printf("xx %s\n",ptr);
         EntryDir* d;
         d=new EntryDir();
         dir->dir_elems[ptr]=d;
         dir=d;
      }
      else
      {
         if(it->second->type==Entry::Node)
         {
            //name necessary for path contains node... cannot continue
            dir=NULL;
            break;
         }
         else
         {
            dir=it->second->as_dir();
         }
      }
      ptr=strtok_r(NULL,"/",&strtok_ctx);
   }
   free(path);
   return dir;
}



         
Manager::Entry* Manager::find_entry(const std::string& in_path,std::string& remainder)
{
   EntryDir* dir=root->as_dir();
   Entry* result=root;
   char* path=strdup(in_path.c_str());
   char* strtok_ctx;
   char* ptr;
   remainder="";
   ptr=strtok_r(path,"/",&strtok_ctx);
   while(ptr!=NULL)
   {
      DirEntryMap::iterator it;
      it=dir->dir_elems.find(ptr);
      if(it==dir->dir_elems.end())
      {
         //requested item not found
         result=NULL;
         break;
      }
      if(it->second->type==Entry::Node)
      {
         //ptr=strtok_r(NULL,"/",&strtok_ctx);
         //if(ptr!=NULL)remainder=ptr; else remainder="";
         if(strtok_ctx!=NULL)
            remainder=strtok_ctx;
         else
            remainder="";
         //remainder=strtok_ctx;
         result=it->second;
         break;
      }
      dir=it->second->as_dir();
      result=it->second;
      ptr=strtok_r(NULL,"/",&strtok_ctx);
   }
   free(path);
   return result;
}
      
         
   //   node: /path
   //   open: /path/ /path

Peek* Manager::do_open(const std::string& in_name)
{
   Peek* peek=NULL;
   //EntryDir* dir;
   Entry* entry;
   std::string remainder;
   pthread_mutex_lock(&structure_lock);
   entry=find_entry(in_name,remainder);
   //dir=find_dir(in_path);
   
   if(entry==NULL)
      goto unlock;
   
   if(entry->type==Entry::Dir)
   {
      //it is dir, create node to list
      //in-place open
      peek=new Peek(&DirListCTX,entry);
      //*peek->ctx=DirListCTX;
      peek->ctx.common=this;
      peek->ctx.instance=entry;
      //entry->add_peek(peek);
   }
   else
   {
      EntryNode* n;
      n=entry->as_node();
      CTX* ctx=&n->base_ctx;
      if(ctx->open==NULL&&remainder!="")
         goto unlock; //open must exist if subnode is opened
      if(ctx->open==NULL)
      {
         peek=new Peek(ctx,n);
         goto unlock;
      }
      int res;
      res=entry->api_begin(); //protect node until open() finishes against unregister
      assert(res==1);         //it should be impossible to get entry that is about to be unregistered
      peek=new Peek(ctx,entry);
      pthread_mutex_unlock(&structure_lock);
      res=peek->ctx.open(&peek->ctx, remainder);
      pthread_mutex_lock(&structure_lock);
      if(res==0)
      {
         //success
      }
      else
      {
         //failure to perform open
         entry->remove_peek(peek);
         delete peek;
         peek=NULL;
      }
      entry->api_end(); //now potential unregister can continue            
   }
   unlock:
   pthread_mutex_unlock(&structure_lock);
   exit:
   return peek;
}
         
         
void Manager::do_close(Peek* p)
{
   //how to make sure only ONE close is executed?
   if(p->entry->api_begin()) //notice this becomes false if forced close was executed
   {
      //bip! forced close may happen now
      if(p->ctx.close!=NULL)
      {
         p->ctx.close(&p->ctx);
      }               
      p->entry->api_end();
   }
   pthread_mutex_lock(&structure_lock);
   p->entry->remove_peek(p);
   pthread_mutex_unlock(&structure_lock);
   delete p;
}

   

int Manager::register_node_internal(const std::string& in_path, const CTX* ctx)
{
   //check if ctx is sane...
   //todo...
   //EntryDir* find_dir(const std::string& in_path)
   EntryDir* dir;
   std::string path;
   std::string name;
   size_t pos;
   pos=in_path.find_last_of('/');         
   if(pos!=std::string::npos)
   {
      path=in_path.substr(0,pos);
      name=in_path.substr(pos+1);
      if(name=="") return -1;
      //printf(";;%s;;%s\n",path.c_str(),name.c_str());
      dir=create_dir_structure(path);
   }         
   else
   {
      name=in_path;
      dir=root->as_dir();
   }
   if(dir->dir_elems.find(name)!=dir->dir_elems.end())
   {
      //some element occupies this
      return -1;
   }
   EntryNode* en;
   en=new EntryNode();
   //en->in_use_cnt=0;
   en->base_ctx=*ctx;
   dir->dir_elems[name]=en;

   
   return 0;
}


int Manager::unregister_node_internal(const std::string& in_path)
{
   vector<string> parts;
   char* path=strdup(in_path.c_str());
   char* strtok_ctx;
   char* ptr;
   ptr=strtok_r(path,"/",&strtok_ctx);
   while(ptr!=NULL)
   {
      parts.push_back(ptr);
      ptr=strtok_r(NULL,"/",&strtok_ctx);
   }
   free(path);
   
   EntryDir* dir=root->as_dir();

   vector<EntryDir*> visited_dirs;
   size_t i;
   visited_dirs.push_back(dir); //push root;
   for(i=0;i<parts.size()-1;i++)
   {
      DirEntryMap::iterator it;
      it=dir->dir_elems.find(parts[i]);
      if(it==dir->dir_elems.end())
         return -1;//cannot locate path
      if(it->second->type!=Entry::Dir)
         return -1;//found element is not dir
      
      dir=it->second->as_dir();
      visited_dirs.push_back(dir);
   }
   
   DirEntryMap::iterator it;
   it=dir->dir_elems.find(parts[i]);
   if(it==dir->dir_elems.end())
      return -1;
   if(it->second->type!=Entry::Node)
      return -1;
   
   //remove node from tree
   EntryNode* n;
   n=it->second->as_node();
   dir->dir_elems.erase(it);
   
   //try do delete tree leading to removed node
   for(i=visited_dirs.size()-1;i>0;i--)
   {
      if(visited_dirs[i]->dir_elems.size()!=0)
         break;
      visited_dirs[i-1]->dir_elems.erase(parts[i-1]);
      visited_dirs[i]->unref();
   }

   int cnt;
   cnt=__sync_fetch_and_or(&n->api_use_cnt,0x1); //signalling that node is removed
   assert((cnt&0x1)==0x0);
   if(cnt>=4)
   {
      //some api action in progress
      pthread_mutex_unlock(&structure_lock);
      //wait until last api exits
      sem_wait(&n->api_cleared);      
      pthread_mutex_lock(&structure_lock);
   }
   for(list<Peek*>::iterator it=n->peeks.begin();it!=n->peeks.end();it++)
   {
      if((*it)->ctx.close!=NULL)
      {
         (*it)->ctx.close(&(*it)->ctx);
      }
   }
   n->unref();
   //if(n->peeks.size()==0)
   //   delete n; //no one uses this entry
   return 0;
}

int Manager::register_node(const std::string& in_path,const CTX* ctx)
{
   int result;
   pthread_mutex_lock(&structure_lock);
   result=register_node_internal(in_path,ctx);
   pthread_mutex_unlock(&structure_lock);
   return result;
}
int Manager::unregister_node(const std::string& in_path)
{
   int result;
   pthread_mutex_lock(&structure_lock);
   result=unregister_node_internal(in_path);
   pthread_mutex_unlock(&structure_lock);
   return result;
}
         
         

Manager* Manager::instance=NULL;



   


Peek* Peek::open(const std::string& path)
{
   Peek* result;
   result=Manager::getInstance().do_open(path);
   return result;
}

void Peek::close()
{
   Manager::getInstance().do_close(this);
}

int Peek::read(std::string& output)
{
   int result=NotImplemented;
   if(ctx.read!=NULL)
   {
      result=NodeAlreadyClosed;
      if(entry->api_begin())
      {
         result=ctx.read(&ctx,output);
         entry->api_end();
      }
   }
   return result;
}

int Peek::write(const std::string& input)
{
   int result=NotImplemented;
   if(ctx.write!=NULL)
   {
      result=NodeAlreadyClosed;
      if(entry->api_begin())
      {
         result=ctx.write(&ctx,input);
         entry->api_end();
      }
   }
   return result;
}

int Peek::list(std::vector< dir_item_d >& files)
{
   int result=NotImplemented;
   if(ctx.dirlist!=NULL)
   {
      result=NodeAlreadyClosed;
      if(entry->api_begin())
      {
         result=ctx.dirlist(&ctx,files);
         entry->api_end();
      }
   }
   return result;    
}
      
unsigned int Peek::flags()
{
   unsigned int f=0;
   if(ctx.dirlist!=NULL) f|=F_DIR;
   if(ctx.read!=NULL) f|=F_RD;
   if(ctx.write!=NULL) f|=F_WR;
   return f;
}
Peek::Peek(CTX* in_ctx,Manager::Entry* base_entry)
{
   ctx=*in_ctx;
   entry=base_entry;
   entry->add_peek(this);
};
Peek::~Peek()
{   
};

   CTX lm::Manager::DirListCTX
         =
         {
            NULL,
            NULL,
            NULL,
            DirList_dirlist,
            DirList_close,
            NULL, //Manager*
            NULL  //dir data
         };

   
}


int lm_register_node(const std::string& in_path, const lm::CTX* ctx)
{
   int result;
   result=lm::Manager::getInstance().register_node(in_path,ctx);
   return result;
}
int lm_unregister_node(const std::string& in_path)
{
   int result;
   result=lm::Manager::getInstance().unregister_node(in_path);
   return result;
}
