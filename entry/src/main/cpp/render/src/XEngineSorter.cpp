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
    HMS_XEG_CreateHPS(context->device_, &info, &sorter2_);
}

void XEngineSorter::cmdDispatchSort(VkCommandBuffer cmdBuffer,
                                   std::shared_ptr<Buffer> depthBuffer, std::shared_ptr<Buffer> tileBuffer, 
                                   std::shared_ptr<Buffer> indexBuffer,
                                   std::shared_ptr<Buffer> sortCount) {

//    XEG_HPSRadixSortDescription sort1Description{
//         XEG_STRUCTURE_TYPE_HPS_RADIX_SORT_DESCRIPTION,
//         nullptr,
//         sortCount->vkBuffer,
//         depthBuffer->vkBuffer,
//         indexBuffer->vkBuffer,
//    };
//    HMS_XEG_CmdRadixSortHPS(cmdBuffer, sorter_, &sort1Description);
//    
//    VkMemoryBarrier barrier = {};
//    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
//    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
//    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
//
//    vkCmdPipelineBarrier(
//        cmdBuffer,
//        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//        0,
//        1, &barrier, 
//        0, nullptr, 
//        0, nullptr
//    );

    XEG_HPSRadixSortDescription sort2Description{
        XEG_STRUCTURE_TYPE_HPS_RADIX_SORT_DESCRIPTION,
        nullptr,
        sortCount->vkBuffer,
        tileBuffer->vkBuffer,
        indexBuffer->vkBuffer,
    };
    HMS_XEG_CmdRadixSortHPS(cmdBuffer, sorter2_, &sort2Description);
}

XEngineSorter::~XEngineSorter() {
    if(sorter_){
        HMS_XEG_DestroyHPS(sorter_);
    }
    if(sorter2_){
        HMS_XEG_DestroyHPS(sorter2_);
    }   
}