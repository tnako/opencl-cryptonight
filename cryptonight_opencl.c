#include <x86intrin.h>
#include <CL/opencl.h>
#include <pthread.h>
#include "cryptonight.h"

__thread cl_context ctx;
__thread cl_kernel krnl = NULL;
__thread cl_program prgm;
__thread cl_platform_id pid;
__thread cl_device_id devid;
__thread cl_mem in, out, lstate;

__thread cl_uint platforms, devs;
__thread cl_command_queue cmdqueue;
__thread cl_context_properties prop[3];

unsigned char do_gpu(void *output, uint8_t *input)
{
	int i;
	FILE *cl;
	cl_int err;
	unsigned char ret;
	size_t global, local;
	char *buf;
	
	
	if(!krnl)
	{
		buf = (char *)malloc(sizeof(char) * (1 << 21));
		cl = fopen("cn-full.cl", "r");
		fread(buf, sizeof(char), 1 << 21, cl);
		fclose(cl);
		
		err = clGetPlatformIDs(1, &pid, &platforms);
		
		if(err != CL_SUCCESS)
		{
			printf("clGetPlatformIDs() returned %d!\n", err);
			exit(0);
		}
		
		err = clGetDeviceIDs(pid, CL_DEVICE_TYPE_GPU, 1, &devid, &devs);
		
		if(err != CL_SUCCESS)
		{
			printf("clGetDeviceIDs() returned %d!\n", err);
			exit(0);
		}
		
		prop[0] = CL_CONTEXT_PLATFORM;
		prop[1] = (cl_context_properties)pid;
		prop[2] = 0x00;
		
		ctx = clCreateContext(prop, 1, &devid, NULL, NULL, &err);
		
		if(err != CL_SUCCESS)
		{
			printf("clCreateContext() returned %d!\n", err);
			exit(0);
		}
		
		cmdqueue = clCreateCommandQueue(ctx, devid, 0, &err);
		prgm = clCreateProgramWithSource(ctx, 1, (const char **)&buf, NULL, &err);
		
		if(err != CL_SUCCESS)
		{
			printf("clCreateProgramWithSource() returned %d!\n", err);
			exit(0);
		}
		
		err = clBuildProgram(prgm, 0, NULL, "-cl-fast-relaxed-math", NULL, NULL);
		
		if(err != CL_SUCCESS)
		{
			char log[1 << 19];
			clGetProgramBuildInfo(prgm, devid, CL_PROGRAM_BUILD_LOG, 1 << 19, log, NULL);
			printf("clBuildProgram() returned %d!\n", err);
			printf("%s\n", log);
			exit(0);
		}
		
		krnl = clCreateKernel(prgm, "aes", &err);
		
		in = clCreateBuffer(ctx, CL_MEM_READ_ONLY, 128, NULL, NULL);
		out = clCreateBuffer(ctx, CL_MEM_READ_WRITE, sizeof(union cn_slow_hash_state), NULL, NULL);
		lstate = clCreateBuffer(ctx, CL_MEM_READ_WRITE, 1 << 21, NULL, NULL);
		free(buf);
	}
	
	err = clEnqueueWriteBuffer(cmdqueue, in, CL_TRUE, 0, 128, input, 0, NULL, NULL);
	
	if(err != CL_SUCCESS)
	{
		printf("clEnqueueWriteBuffer() returned %d!\n", err);
		exit(0);
	}
	
	clSetKernelArg(krnl, 0, sizeof(cl_mem), &out);
	clSetKernelArg(krnl, 1, sizeof(cl_mem), &in);
	clSetKernelArg(krnl, 2, sizeof(cl_mem), &lstate);
	
	global = 1;
	local = 1;
	err = clEnqueueNDRangeKernel(cmdqueue, krnl, 1, NULL, &global, &local, 0, NULL, NULL);
	
	if(err != CL_SUCCESS)
	{
		printf("clEnqueueNDRangeKernel() returned %d!\n", err);
		exit(0);
	}
	
	clFlush(cmdqueue);
	clFinish(cmdqueue);
		
	err = clEnqueueReadBuffer(cmdqueue, out, CL_TRUE, 0, sizeof(union cn_slow_hash_state), output, 0, NULL, NULL);
	
	if(err != CL_SUCCESS)
	{
		printf("clEnqueueReadBuffer() returned %d!\n", err);
		exit(0);
	}
	
	clFlush(cmdqueue);
	clFinish(cmdqueue);
	
	/*clReleaseMemObject(in);
	clReleaseMemObject(out);
	clReleaseMemObject(key1);
	clReleaseMemObject(key2);
	clReleaseMemObject(amem);
	clReleaseMemObject(bmem);
	clReleaseMemObject(lstate);
	clReleaseProgram(prgm);
	clReleaseKernel(krnl);
	clReleaseCommandQueue(cmdqueue);
	clReleaseContext(ctx);*/
	return(0);
}
	
void cryptonight_hash_ctx(void *restrict output, const void *restrict input, struct cryptonight_ctx *restrict cnctx)
{  
	unsigned char ret;
    do_gpu(&cnctx->state, input);
    extra_hashes[cnctx->state.hs.b[0] & 3](&cnctx->state, 200, output);
}

