#ifndef LIGHTMON_H
#define LIGHTMON_H
/**
   \defgroup lm_group LM monitoring filesystem

   \brief Light filesystem-like organizer for monitoring software state
   @{
 */

#include <string>
#include <vector>
#include <stdio.h>
namespace lm
{
  typedef struct
  {
    std::string name;
    unsigned int flags;
  } dir_item_d;

  typedef enum
  {
    MoreData=1,
    Success=0,
    NotImplemented=-1,
    NodeAlreadyClosed=-2
  } result_e;

  static const unsigned int F_RD = 1;
  static const unsigned int F_WR = 2;
  static const unsigned int F_DIR = 4;

  inline bool isdir(unsigned int flags)
  {
    return (flags&F_DIR) == F_DIR;
  }
  inline bool isreadable(unsigned int flags)
  {
    return (flags&F_RD) == F_RD;
  }
  inline bool iswritable(unsigned int flags)
  {
    return (flags&F_WR) == F_WR;
  }

  typedef struct CTX
  {
    /**
     * function pointer. function is called when DataNode is accessed from Manager(GETNODE) and when accessed DataNode
     * is open to write or read.
     * after open function was called pointers to other function must indicate whether the node is a file or directory.
     * for example if there is dirlist function set there should not be present read or write function pointer and opposite.
     * open can also change functions pointer to others functions pointer.
     * open can use instance_ctx and instance_str, but if some memory is alocated in open function then close function
     * must free this memory to avoid leaks.
     *
     * @see Register.h
     * @param ctx	to function is passed copy of this structure registred in Manager(REGISTER)
     * @param path	to function is passed subpath to opend path. in simple case it should be "/"
     * @return	-1 if node cannot be opened.
     *			 0 if node is opened succesfuly
     */
    result_e (*open)	(CTX* ctx,const std::string& path);

    /**
     * function pointer. function is called when DataNode after opening is readed.
     * value returned by DataNode read function is the same as retuned by this function.
     *
     * @see Register.h
     * @param ctx	to function is passed copy of this structure registred in Manager(REGISTER)
     * @param value	std::string to which data should be writen;
     * @return	-1  if node cannot be readed. UnknownError
     *			>=0 if node is read succesfuly.
     */
    result_e (*read)	(CTX* ctx, std::string& value);

    /**
     * function pointer. function is called when DataNode after opening is writen.
     * value retured by DataNode write function is the same as retuned by this function.
     *
     * @see Register.h
     * @param ctx	to function is passed copy of this structure registred in Manager(REGISTER)
     * @param value	std::string from which data should be readed;
     * @return	-1  if node cannot be writed. UnknownError
     *			>=0 if node is writen succesfuly.
     */
    result_e (*write) (CTX* ctx, const std::string& value);

    /**
     * function pointer. function is called when DataNode after listed.
     * value retured by DataNode list function is the same as retuned by this function.
     *
     * @see Register.h
     * @see DataListItem.h
     * @param ctx	to function is passed copy of this structure registred in Manager(REGISTER)
     * @param files	std::list to which list of subfiles should be added;
     * @return	-1  if node cannot be writed. UnknownError
     *			>=0 if node is writen succesfuly.
     */
    result_e (*dirlist) (CTX* ctx, std::vector<dir_item_d>& files);

    /**
     * function pointer. function is called when DataNode is accessed from Manager(GETNODE) and when accessed DataNode
     * is close after write or read.
     * if memory was alocated in open function then close function must free this memory.
     *
     * @see Register.h
     * @param ctx	to function is passed copy of this structure registred in Manager(REGISTER)
     * @return	-1 if node cannot be opened.
     *			0  if node is opened succesfuly
     */
    void (*close) (CTX* ctx);

    /**
     * instance context lcan be allocated in open function, if that happen it must be free in close function. not used by Manager or DataNode.
     * @see Register.h
     */
    void* common;

    /**
     * node_data can be used to pass ' this ' pointer to registred functions. not used by Manager or DataNode.
     * @see Register.h
     */
    void* instance;
  } CTX;


  int register_node(const std::string& path, const CTX* ctx);
  int unregister_node(const std::string& path);


  /**
   Description:
   Peek is accessor class intended for readers of data managed by LM.
   Data managed by LM is organized in structure similar to filesystem tree.
   */
#ifndef COMPILING_LM
  class Peek
  {
  public:
    /**
     * Opens reference to LM managed element. It can specify directory, data node, or even subpath inside data node.
     * @see LM tree
     *
     * @param	path - string with fully-qualified file or dir
     * @return	accessor for requested file; NULL if path cannot be recognized
     */
    static Peek* open(const std::string& path);
    /**
     * Closes accessor and deletes object.
     */
    void close();
    /**
     * read function. open should be called before read.
     *
     * @see Register.h
     * @param	string where data will be written
     * @return	@ref Error
     */
    result_e read(std::string& );

    /**
     * write function. open should be called before write.
     *
     * @see Register.h
     * @param	string from which data will be readed
     * @return	error code
     */
    result_e write(const std::string& );
    /**
     * list function. return list of sub nodes.
     *
     * @see Register.h
     * @see DataListItem.h
     * @param	list  where data about sub nodes will be added
     * @return	error code
     */
    result_e list(std::vector< dir_item_d >&);
    /**
     * Retrieves bitflags that describe properties of opened file.
     * These flags can be tested with @ref isdir, @ref isreadable, @ref iswritable.
     */
    unsigned int flags();
  private:
    Peek(){};
    ~Peek(){};
  };

  /**
   @}
   */

#ifdef __GNUC__
#include <dlfcn.h>
  extern "C"
  {
    static int __lm_register_node(const std::string& path, const CTX* ctx) __attribute__((weakref("lm_register_node")));
    static int __lm_unregister_node(const std::string& path) __attribute__((weakref("lm_unregister_node")));
  }

  /*calling is created via class because only g++ does emit weak symbols for local static variabled*/
  class lm_trampoline
  {
  private:
    friend int lm::register_node(const std::string& path, const CTX* ctx);
    friend int lm::unregister_node(const std::string& path);

    static int lm_register(const std::string& path, const CTX* ctx)
    {
      static void* _register_node=dlsym(RTLD_DEFAULT,"lm_register_node");
      if(_register_node!=NULL)
        return ((int (*)(const std::string& path, const lm::CTX* ctx))_register_node)(path,ctx);
      if(__lm_register_node!=NULL)
        return __lm_register_node(path,ctx);
      return -1;
    }
    static int lm_unregister(const std::string& path)
    {
      static void* _unregister_node=dlsym(RTLD_DEFAULT,"lm_unregister_node");
      if(_unregister_node!=NULL)
        return ((int (*)(const std::string& path))_unregister_node)(path);
      if(__lm_unregister_node!=NULL)
        return __lm_unregister_node(path);
      return -1;
    }
  };

  int register_node(const std::string& path, const CTX* ctx) __attribute__ ((weak));
  int register_node(const std::string& path, const CTX* ctx)
  {
    return lm_trampoline::lm_register(path,ctx);
  }
  int unregister_node(const std::string& path) __attribute__ ((weak));
  int unregister_node(const std::string& path)
  {
    return lm_trampoline::lm_unregister(path);
  }
#else
  /*no implementation for non-gcc compiler*/
  int register_node(const std::string& path, const CTX* ctx){return 0;};
  int unregister_node(const std::string& path){return 0;};
#endif

#endif //compiling LM


  //include <stdio.h>
  static inline std::string operator+(std::string left,int number)
  {
    char s[10];
    sprintf(s,"%d",number);
    return left+s;
  }
  static inline std::string operator+(std::string left,unsigned int number)
  {
    char s[10];
    sprintf(s,"%u",number);
    return left+s;
  }
  static inline std::string operator+(std::string left,void* ptr)
  {
    char s[15];
    sprintf(s,"%p",ptr);
    return left+s;
  }
} //namespace

#endif //LM_H
