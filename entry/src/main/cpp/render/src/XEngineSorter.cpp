//
// Created on 2026/4/26.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "XEngineSorter.h"
#include "Context.h"
#include "Buffer.h"

XEngineSorter::XEngineSorter(const std::shared_ptr<Context>& _context)
    : context(_context) {
    XEG_HPSRadixSort sorInfo{
        XEG_STRUCTURE_TYPE_HPS_RADIX_SORT,
        nullptr
    };
    
    XEG_HPSCreateInfo info {
        XEG_STRUCTURE_TYPE_HPS_CREATE_INFO,
        &sorInfo
    };
    
    HMS_XEG_CreateHPS(context->device_, &info, &sorter_);
}

void XEngineSorter::cmdDispatchSort(VkCommandBuffer cmdBuffer,
                                   std::shared_ptr<Buffer> keyBuffer, 
                                   std::shared_ptr<Buffer> valueBuffer,
                                   std::shared_ptr<Buffer> sortCount) {

   XEG_HPSRadixSortDescription sortDescription{
        XEG_STRUCTURE_TYPE_HPS_RADIX_SORT_DESCRIPTION,
        nullptr,
        sortCount->vkBuffer,
        keyBuffer->vkBuffer,
        valueBuffer->vkBuffer,
   };

   HMS_XEG_CmdRadixSortHPS(cmdBuffer, sorter_, &sortDescription);
}

XEngineSorter::~XEngineSorter() {
    if(sorter_){
        HMS_XEG_DestroyHPS(sorter_);
    }
}