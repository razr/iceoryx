// Copyright (c) 2019 - 2020 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2020 - 2021 by Apex.AI Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_posh/mepoo/chunk_header.hpp"
#include "iceoryx_utils/cxx/helplets.hpp"

namespace iox
{
namespace mepoo
{
ChunkHeader::ChunkHeader(const uint32_t chunkSize, const ChunkSettings& chunkSettings) noexcept
{
    static_assert(alignof(ChunkHeader) >= 8U,
                  "All the calculations expect the ChunkHeader alignment to be at least 8!");

    this->chunkSize = chunkSize;
    userPayloadSize = chunkSettings.userPayloadSize();
    const auto userPayloadAlignment = chunkSettings.userPayloadAlignment();
    const auto userHeaderSize = chunkSettings.userHeaderSize();

    static_assert(
        std::is_same<UserPayloadOffset_t, std::remove_const<decltype(userPayloadAlignment)>::type>::value,
        "UserPayloadOffset_t and userPayloadAlignment must have same type in order to prevent an overflow for the "
        "user-payload offset calculation for extremely large alignments");

    // have a look at »User-Payload Offset Calculation« in chunk_header.md for more details regarding the calculation
    if (userHeaderSize == 0U)
    {
        if (userPayloadAlignment <= alignof(mepoo::ChunkHeader))
        {
            // the most simple case with no user-header and the user-payload adjacent to the ChunkHeader
            userPayloadOffset = sizeof(ChunkHeader);
        }
        else
        {
            // the second most simple case with no user-header but the user-payload alignment
            // exceeds the ChunkHeader alignment and is therefore not necessarily adjacent
            uint64_t addressOfChunkHeader = reinterpret_cast<uint64_t>(this);
            uint64_t headerEndAddress = addressOfChunkHeader + sizeof(ChunkHeader);
            uint64_t alignedUserPayloadAddress =
                iox::cxx::align(headerEndAddress, static_cast<uint64_t>(userPayloadAlignment));
            uint64_t offsetToUserPayload = alignedUserPayloadAddress - addressOfChunkHeader;
            // the cast is safe since userPayloadOffset and userPayloadAlignment have the same type and since the
            // alignment must be a power of 2, the max alignment is about half of the max value the type can hold
            userPayloadOffset = static_cast<UserPayloadOffset_t>(offsetToUserPayload);

            // this is safe since the alignment of the user-payload is larger than the one from the ChunkHeader
            // -> the user-payload is either adjacent and `backOffset` is at the same location as `userPayloadOffset`
            //    or the user-payload is not adjacent and there is space of at least the alignment of ChunkHeader
            //    between the ChunkHeader and the user-payload
            auto addressOfBackOffset = alignedUserPayloadAddress - sizeof(UserPayloadOffset_t);
            auto backOffset = reinterpret_cast<UserPayloadOffset_t*>(addressOfBackOffset);
            *backOffset = userPayloadOffset;
        }
    }
    else
    {
        // the most complex case with a user-header
        auto addressOfChunkHeader = reinterpret_cast<uint64_t>(this);
        uint64_t headerEndAddress = addressOfChunkHeader + sizeof(ChunkHeader) + userHeaderSize;
        uint64_t anticipatedBackOffsetAddress =
            iox::cxx::align(headerEndAddress, static_cast<uint64_t>(alignof(UserPayloadOffset_t)));
        uint64_t unalignedUserPayloadAddress = anticipatedBackOffsetAddress + sizeof(UserPayloadOffset_t);
        uint64_t alignedUserPayloadAddress =
            iox::cxx::align(unalignedUserPayloadAddress, static_cast<uint64_t>(userPayloadAlignment));
        uint64_t offsetToUserPayload = alignedUserPayloadAddress - addressOfChunkHeader;
        // the cast is safe since userPayloadOffset and userPayloadAlignment have the same type and since the alignment
        // must be a power of 2, the max alignment is about half of the max value the type can hold
        userPayloadOffset = static_cast<UserPayloadOffset_t>(offsetToUserPayload);

        // this always works if the alignment of UserPayloadOffset_t and userPayloadAlignment are equal,
        // if not there are two options:
        //   - either the alignment of the UserPayloadOffset_t is larger than userPayloadAlignment
        //     -> the userPayload is adjacent to the back-offset and therefore this also works
        //   - or the alignment of the UserPayloadOffset_t is smaller than userPayloadAlignment
        //     -> the back-offset can be put adjacent to to the Topic since the smaller alignment always fits
        auto backOffsetAddress = alignedUserPayloadAddress - sizeof(UserPayloadOffset_t);
        auto backOffset = reinterpret_cast<UserPayloadOffset_t*>(backOffsetAddress);
        *backOffset = userPayloadOffset;
    }

    cxx::Ensures(overflowSafeUsedSizeOfChunk() <= chunkSize
                 && "Used size of chunk would exceed the actual chunk size!");
}

void* ChunkHeader::userPayload() const noexcept
{
    // user-payload is always located relative to "this" in this way
    return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(this) + userPayloadOffset);
}

ChunkHeader* ChunkHeader::fromUserPayload(const void* const userPayload) noexcept
{
    if (userPayload == nullptr)
    {
        return nullptr;
    }
    uint64_t userPayloadAddress = reinterpret_cast<uint64_t>(userPayload);
    // the back-offset is always stored in front of the user-payload, no matter if a user-header is used or not or if
    // the user-payload has a custom alignment
    auto backOffset = reinterpret_cast<UserPayloadOffset_t*>(userPayloadAddress - sizeof(UserPayloadOffset_t));
    return reinterpret_cast<ChunkHeader*>(userPayloadAddress - *backOffset);
}

uint32_t ChunkHeader::usedSizeOfChunk() const noexcept
{
    return static_cast<uint32_t>(overflowSafeUsedSizeOfChunk());
}

uint64_t ChunkHeader::overflowSafeUsedSizeOfChunk() const noexcept
{
    return static_cast<uint64_t>(userPayloadOffset) + static_cast<uint64_t>(userPayloadSize);
}

} // namespace mepoo
} // namespace iox
