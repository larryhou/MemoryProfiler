#pragma once

#ifndef AutoCharVector_h
#define AutoCharVector_h

#include "stdio.h"
#include "stdlib.h"

#if _MSC_VER 
#include "malloc.h"
#endif

class AutoCharVector
{

public:
	AutoCharVector(size_t size)
	{
		mDataSize = size;
		mData = (char*) malloc (size + 1);
	}

	~AutoCharVector()
	{
		free(mData);
	}

	operator char*() {
		return mData;
	}

	operator void*() {
		return mData;
	}

	size_t size() {
		return mDataSize;
	}

	char* p()
	{
		return mData;
	}

private:
	char* mData;
	size_t mDataSize;
};

#endif