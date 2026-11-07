#include "SexyAppFramework/misc/CheckedInteger.h"
#include "SexyAppFramework/misc/Crc32.h"
#include "SexyAppFramework/misc/MTRand.h"
#include <random>
#include <limits>
#include <iostream>
#include <cassert>
bool gInAssert = false;
template<class F> void rejects(F fn) { bool threw=false; try {fn();} catch(const std::overflow_error&) {threw=true;} assert(threw); }
int main() {
 using Sexy::CheckedInteger; using Sexy::LegacyStringIndex;
 assert(CheckedInteger<int>(size_t{0})==0);
 assert(CheckedInteger<int>(size_t{INT32_MAX})==INT32_MAX);
 assert(CheckedInteger<int>(int64_t{INT32_MIN})==INT32_MIN);
 rejects([]{CheckedInteger<int>(uint64_t{INT32_MAX}+1);});
 rejects([]{CheckedInteger<int>(int64_t{INT32_MIN}-1);});
 rejects([]{CheckedInteger<uint32_t>(-1);});
 assert(CheckedInteger<uint32_t>(uint64_t{UINT32_MAX})==UINT32_MAX);
 rejects([]{CheckedInteger<uint32_t>(uint64_t{UINT32_MAX}+1);});
 assert(LegacyStringIndex(std::string_view::npos)==-1);
 assert(LegacyStringIndex(123)==123);
 rejects([]{LegacyStringIndex(size_t{INT32_MAX}+1);});
 // CRC standard check vector and incremental equivalence; same 32-bit on-disk CRC.
 const auto* bytes=reinterpret_cast<const Bytef*>("123456789");
 assert(Sexy::Crc32(0,bytes,9)==0xcbf43926U);
 assert(Sexy::Crc32(Sexy::Crc32(0,bytes,4),bytes+4,5)==0xcbf43926U);
 assert(Sexy::Crc32(0,nullptr,0)==0);
 for (uint32_t seed : {0U,1U,4357U,5489U,UINT32_MAX}) {
  Sexy::MTRand rng(static_cast<unsigned long>(seed)); std::mt19937 reference(seed==0?4357:seed);
  for(int i=0;i<100000;++i) assert(rng.Next()==(reference()&0x7fffffffU));
  auto serialized=rng.Serialize(); assert(serialized.size()==624*4+4);
  Sexy::MTRand restored(serialized);
  for(int i=0;i<10000;++i) assert(rng.Next()==restored.Next());
 }
 std::cout << "PASS: checked integer boundaries, npos, CRC32 standard/incremental vectors, 500000 MT19937 reference outputs, 50000 serialized continuation outputs\n";
}
