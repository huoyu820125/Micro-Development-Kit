#ifndef SINGLETION_H_
#define SINGLETION_H_

///////////////////////////////////////////////////////////////////////////
//µ¥ÀýÄ£°å
//////////////////////////////////////////////////////////////////////////
#include <assert.h>

namespace mdk
{
/// Should be placed in the appropriate .cpp file somewhere
#define initialiseSingleton( type ) \
	template <> type * Singleton < type > :: mSingleton = 0

/// To be used as a replacement for initialiseSingleton( )
///  Creates a file-scoped Singleton object, to be retrieved with getSingleton
#define CreateSingleton( type ) \
	initialiseSingleton( type ); \
	type the##type

template < class type > 
class Singleton
{
public:
	/// Constructor
	Singleton()
	{
		/// If you hit this assert, this singleton already exists -- you can't create another one!
		assert(this->mSingleton == 0);
		this->mSingleton = static_cast<type*>(this);
	}
	/// Destructor
	virtual ~Singleton()
	{
		this->mSingleton = 0;
	}

	static type & GetSingleton() { assert(mSingleton); return *mSingleton; }
	static type* GetSingletonPtr() { return mSingleton; }

protected:

	/// Singleton pointer, must be set to 0 prior to creating the object
	static type* mSingleton;
};
};//---mdk --- end---
#endif