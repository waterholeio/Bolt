#pragma once

#include <bolt/cl/bolt.h>
#include <mutex>
#include <string>
#include <iostream>
#include <numeric>

namespace bolt {
    namespace cl {
        //  The following two functions disallow non-random access functions

        // Wrapper that uses default control class, iterator interface
        template<typename T, typename InputIterator, typename UnaryFunction, typename BinaryFunction> 
        T transform_reduce(const control &ctl, InputIterator first, InputIterator last,
            UnaryFunction transform_op,
            T init,  BinaryFunction reduce_op, const std::string user_code, std::input_iterator_tag )
        {
            //  TODO:  It should be possible to support non-random_access_iterator_tag iterators, if we copied the data 
            //  to a temporary buffer.  Should we?
            throw ::cl::Error( CL_INVALID_OPERATION, "Transform_reduce currently only supports random access iterator types" );
        };

        // Wrapper that uses default control class, iterator interface
        template<typename T, typename InputIterator, typename UnaryFunction, typename BinaryFunction> 
        T transform_reduce(const control& ctl, InputIterator first, InputIterator last,
            UnaryFunction transform_op,
            T init,  BinaryFunction reduce_op, const std::string user_code, std::random_access_iterator_tag )
        {
            return detail::transform_reduce( ctl, first, last, transform_op, init, reduce_op, user_code);
        };

        // The following two functions are visible in .h file
        // Wrapper that user passes a control class
        template<typename T, typename InputIterator, typename UnaryFunction, typename BinaryFunction> 
        T transform_reduce(const control& ctl, InputIterator first, InputIterator last,  
            UnaryFunction transform_op, 
            T init,  BinaryFunction reduce_op, const std::string user_code )  
        {
            return transform_reduce( ctl, first, last, transform_op, init, reduce_op, user_code, 
                std::iterator_traits< InputIterator >::iterator_category( ) );
        };

        // Wrapper that generates default control class
        template<typename T, typename InputIterator, typename UnaryFunction, typename BinaryFunction> 
        T transform_reduce(InputIterator first, InputIterator last,
            UnaryFunction transform_op,
            T init,  BinaryFunction reduce_op, const std::string user_code )
        {
            return transform_reduce(control::getDefault(), first, last, transform_op, init, reduce_op, user_code, 
                std::iterator_traits< InputIterator >::iterator_category( ) );
        };
    };
};


namespace bolt {
    namespace cl {
        namespace  detail {
            struct CallCompiler_TransformReduce {
                static void constructAndCompile(::cl::Kernel *masterKernel,  std::string user_code, std::string valueTypeName,  std::string transformFunctorTypeName, std::string reduceFunctorTypeName) {

                    const std::string instantiationString = 
                        "// Host generates this instantiation string with user-specified value type and functor\n"
                        "template __attribute__((mangled_name(transform_reduceInstantiated)))\n"
                        "kernel void transform_reduceTemplate(\n"
                        "global " + valueTypeName + "* A,\n"
                        "const int length,\n"
                        "global " + transformFunctorTypeName + "* transformFunctor,\n"
                        "const " + valueTypeName + " init,\n"
                        "global " + reduceFunctorTypeName + "* reduceFunctor,\n"
                        "global " + valueTypeName + "* result,\n"
                        "local " + valueTypeName + "* scratch\n"
                        ");\n\n";

                    std::string functorNames = transformFunctorTypeName + " , " + reduceFunctorTypeName; // create for debug message

                    bolt::cl::control c = bolt::cl::control::getDefault();  // FIXME- this needs to be passed a parm but we have too many arguments for call_once
                    bolt::cl::constructAndCompile(masterKernel, "transform_reduce", instantiationString, user_code, valueTypeName, functorNames, c);

                };
            };

        // This template is called by the non-detail versions of transform_reduce, it already assumes random access iterators
        // This is called strictly for any non-device_vector iterator
        template<typename T, typename InputIterator, typename UnaryFunction, typename BinaryFunction> 
        typename std::enable_if< !std::is_base_of<typename device_vector<typename std::iterator_traits<InputIterator>::value_type>::iterator,InputIterator>::value, T >::type
        transform_reduce(const control &c, InputIterator first, InputIterator last, UnaryFunction transform_op, 
            T init,  BinaryFunction reduce_op, const std::string user_code )
        {
            typedef std::iterator_traits<InputIterator>::value_type T;

            const bolt::cl::control::e_RunMode runMode = c.forceRunMode();  // could be dynamic choice some day.
            if (runMode == bolt::cl::control::SerialCpu)
            {
                size_t szElements = last - first; 
                //Create a temporary array to store the transform result;
                std::vector<T> output(szElements);

                std::transform(first, last, output.begin(),transform_op);
                return std::accumulate(output.begin(), output.end(), init);

            } else if (runMode == bolt::cl::control::MultiCoreCpu) {
                std::cout << "The MultiCoreCpu version of transform_reduce is not implemented yet." << std ::endl;
                return init;
            } else {
                // Map the input iterator to a device_vector
                device_vector< T > dvInput( first, last, CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, c );

                return  detail::transform_reduce_enqueue( c, dvInput.begin( ), dvInput.end( ), transform_op, init, reduce_op, user_code );
            }
        };

        // This template is called by the non-detail versions of transform_reduce, it already assumes random access iterators
        // This is called strictly for iterators that are derived from device_vector< T >::iterator
        template<typename T, typename InputIterator, typename UnaryFunction, typename BinaryFunction> 
        typename std::enable_if< std::is_base_of<typename device_vector<typename std::iterator_traits<InputIterator>::value_type>::iterator,InputIterator>::value, T >::type
        transform_reduce(const control &c, InputIterator first, InputIterator last, UnaryFunction transform_op, 
            T init,  BinaryFunction reduce_op, const std::string user_code )
        {
            typedef std::iterator_traits<InputIterator>::value_type T;

            const bolt::cl::control::e_RunMode runMode = c.forceRunMode();  // could be dynamic choice some day.
            if (runMode == bolt::cl::control::SerialCpu)
            {
                //  TODO:  Need access to the device_vector .data method to get a host pointer
                throw ::cl::Error( CL_INVALID_DEVICE, "transform_reduce device_vector CPU device not implemented" );

                size_t szElements = last - first; 
                //Create a temporary array to store the transform result;
                std::vector<T> output(szElements);

                std::transform(first, last, output.begin(),transform_op);
                return std::accumulate(output.begin(), output.end(), init);

            }
            else if (runMode == bolt::cl::control::MultiCoreCpu)
            {
                //  TODO:  Need access to the device_vector .data method to get a host pointer
                throw ::cl::Error( CL_INVALID_DEVICE, "transform_reduce device_vector CPU device not implemented" );

                std::cout << "The MultiCoreCpu version of transform_reduce is not implemented yet." << std ::endl;
                return init;
            }

            return  detail::transform_reduce_enqueue( c, first, last, transform_op, init, reduce_op, user_code );
        };

            template<typename T, typename InputIterator, typename UnaryFunction, typename BinaryFunction> 
            T transform_reduce_enqueue(const control& c, InputIterator first, InputIterator last, UnaryFunction transform_op,
                T init, BinaryFunction reduce_op, const std::string user_code="")
            {
                static std::once_flag initOnlyOnce;
                static  ::cl::Kernel masterKernel;
                unsigned debugMode = 0; //FIXME, use control

                std::call_once(initOnlyOnce, CallCompiler_TransformReduce::constructAndCompile, &masterKernel, 
                    "\n//--user Code\n" + user_code + "\n//---Functions\n" + ClCode<UnaryFunction>::get() + ClCode<BinaryFunction>::get() , 
                    TypeName<T>::get(), TypeName<UnaryFunction>::get(), TypeName<BinaryFunction>::get());


                // Set up shape of launch grid and buffers:
                // FIXME, read from device attributes.
                int computeUnits     = c.device().getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();  // round up if we don't know. 
                int wgPerComputeUnit =  c.wgPerComputeUnit(); 
                int resultCnt = computeUnits * wgPerComputeUnit;
                const int wgSize = 64; 

                // Create Buffer wrappers so we can access the host functors, for read or writing in the kernel
                ::cl::Buffer transformFunctor(c.context(), CL_MEM_USE_HOST_PTR, sizeof(transform_op), &transform_op );   
                ::cl::Buffer reduceFunctor(c.context(), CL_MEM_USE_HOST_PTR, sizeof(reduce_op), &reduce_op );
                ::cl::Buffer result(c.context(), CL_MEM_ALLOC_HOST_PTR|CL_MEM_WRITE_ONLY, sizeof(T) * resultCnt);

                ::cl::Kernel k = masterKernel;  // hopefully create a copy of the kernel. FIXME, doesn't work.

                cl_uint szElements = static_cast< cl_uint >( std::distance( first, last ) );
                
                /***** This is a temporaray fix *****/
                /*What if  requiredWorkGroups > resultCnt? Do you want to loop or increase the work group size or increase the per item processing?*/
                int requiredWorkGroups = (int)ceil((float)szElements/wgSize); 
                if (requiredWorkGroups < resultCnt)
                    resultCnt = requiredWorkGroups;
                /**********************/

                k.setArg(0, (*first).getBuffer( ) );
                k.setArg(1, szElements);
                k.setArg(2, transformFunctor);
                k.setArg(3, init);
                k.setArg(4, reduceFunctor);
                k.setArg(5, result);
                ::cl::LocalSpaceArg loc;
                loc.size_ = wgSize*sizeof(T);
                k.setArg(6, loc);

                // FIXME.  Need to ensure global size is a multiple of local WG size ,etc.
                // FIXME. Need to ensure that only required amount of work groups are spawned.
                c.commandQueue().enqueueNDRangeKernel( 
                    k, 
                    ::cl::NullRange, 
                    ::cl::NDRange(resultCnt * wgSize), 
                    ::cl::NDRange(wgSize),
                                NULL,
                                NULL);
                c.commandQueue().finish();
                // FIXME: replace with map:
                // FIXME: Note this depends on supplied functor having a version which can be compiled to run on the CPU
                std::vector<T> outputArray(resultCnt);

                T *h_result = (T*)c.commandQueue().enqueueMapBuffer(result, true, CL_MAP_READ, 0, sizeof(T)*resultCnt);
                T acc = init;
                for(int i = 0; i < resultCnt; ++i){
                    acc = reduce_op(h_result[i], acc);
                }
                return acc;
            };
        }// end of namespace detail
    }// end of namespace cl
}// end of namespace bolt

// FIXME -review use of string vs const string.  Should TypeName<> return a std::string?
// FIXME - add line numbers to pretty-print kernel log file.
// FIXME - experiment with passing functors as objects rather than as parameters.  (Args can't return state to host, but OK?)