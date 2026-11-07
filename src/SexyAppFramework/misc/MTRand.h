#ifndef __MTRAND_H__
#define __MTRAND_H__

#include <string>
#include <stdint.h>

namespace Sexy
{

constexpr const int MTRAND_N = 624;

class MTRand
{
	uint32_t mt[MTRAND_N]; /* the array for the state vector  */
	int mti;

public:
	MTRand(const std::string& theSerialData);
	MTRand(unsigned long seed);
	MTRand();

	void SRand(const std::string& theSerialData);
	void SRand(unsigned long seed);
	uint32_t NextNoAssert();
	uint32_t Next();
	uint32_t NextNoAssert(unsigned long range);
	uint32_t Next(unsigned long range);
	float NextNoAssert(float range);
	float Next( float range );

	std::string Serialize();

	static void SetRandAllowed(bool allowed);
};

struct MTAutoDisallowRand
{
	MTAutoDisallowRand() { MTRand::SetRandAllowed(false); }
	~MTAutoDisallowRand() { MTRand::SetRandAllowed(true); }
};

}

#endif //__MTRAND_H__
