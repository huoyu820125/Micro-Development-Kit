#ifndef MACRODEFINE_H_
#define MACRODEFINE_H_

//宏定义，其实宏定义不受命名空间约束，只是为了统一才写的
namespace mdk
{

#ifndef SAFE_FREE
#define SAFE_FREE(p)	{if(p) free(p);p=NULL;}
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)	{if(p) delete p;p=NULL;}
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(ptr) {if(ptr){delete []ptr;ptr=NULL;}}
#endif

//简单的实现
#ifndef SYNTHESIZE
#define SYNTHESIZE(varType, varName, funName)\
protected: varType varName;\
public: virtual varType get##funName(void) const { return varName; }\
public: virtual void set##funName(varType var){ varName = var; }
#endif

};
#endif //MACRODEFINE_H_
