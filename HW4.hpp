#ifndef __HW4_HPP__
#define __HW4_HPP__

#include <stdexcept>
#include <iostream>
#include <fstream>

using std::cerr;
using std::endl;
using std::ostream;
using std::right;
using std::setw;

#define KB 1024											 // 2^10 bytes
#define BLOCK_SIZE 64									 // 64 bytes
#define L1_SET_WAYS 8									 // Number of ways in L1 cache
#define L2_SET_WAYS 16									 // Number of ways in L2 cache
#define L1_SIZE 64 * KB									 // 64 KB
#define L2_SIZE 1024 * KB								 // 1 MB
#define L1_SET_SIZE (L1_SIZE / BLOCK_SIZE / L1_SET_WAYS) // Number of sets in L1 cache
#define L2_SET_SIZE (L2_SIZE / BLOCK_SIZE / L2_SET_WAYS) // Number of sets in L2 cache
#define L1_IDX(block) (block % L1_SET_SIZE)				 // L1 set index
#define L2_IDX(block) (block % L2_SET_SIZE)				 // L2 set index

typedef unsigned long long timee_t;
typedef unsigned long block_t;

class CacheLine
{
public:
	int valid = 0;	   // valid bit
	int inl1;		   // in L1
	unsigned hits = 0; // number of hits in L2
	block_t block;	   // TAG
	union
	{
		timee_t time;  // LRU time
		unsigned rrpv; // RRPV
		bool ref;	   // REF bit
	};
};

template <unsigned WAYS>
class CacheSet
{
private:
	CacheLine _cache[WAYS];

public:
	CacheLine &find(block_t block_id)
	{
		for (unsigned i = 0; i < WAYS; i++)
		{
			if (_cache[i].valid && _cache[i].block == block_id)
				return _cache[i];
		}

		return _cache[0];
	}

	CacheLine &find_LRU()
	{
		timee_t min_time = _cache[0].time;
		int min_idx = 0;
		for (unsigned i = 1; i < WAYS; i++)
		{
			if (!_cache[i].valid)
				return _cache[i];

			if (_cache[i].time < min_time)
			{
				min_time = _cache[i].time;
				min_idx = i;
			}
		}

		return _cache[min_idx];
	}

	CacheLine &find_SRRIP()
	{
		for (unsigned i = 0; i < WAYS; i++)
		{
			if (!_cache[i].valid)
				return _cache[i];
		}

		for (unsigned i = 0; i < WAYS; i++)
		{
			if (_cache[i].rrpv == 3 && !_cache[i].inl1)
				return _cache[i];
		}

		for (unsigned i = 0; i < WAYS; i++)
		{
			_cache[i].rrpv++;
		}

		return find_SRRIP();
	}

	CacheLine &find_NRU(block_t mru)
	{
		for (unsigned i = 0; i < WAYS; i++)
		{
			if (!_cache[i].valid)
				return _cache[i];
		}

		for (unsigned i = 0; i < WAYS; i++)
		{
			if (!_cache[i].ref && !_cache[i].inl1)
				return _cache[i];
		}

		for (unsigned i = 0; i < WAYS; i++)
		{
			if (_cache[i].block != mru)
				_cache[i].ref = false;
		}

		for (unsigned i = 0; i < WAYS; i++)
		{
			if (!_cache[i].ref && !_cache[i].inl1)
				return _cache[i];
		}

		return _cache[0];
	}

	void debug()
	{
		for (unsigned i = 0; i < WAYS; i++)
		{
			if (_cache[i].valid)
				cerr << i << " " << _cache[i].block << " " << _cache[i].rrpv << " " << _cache[i].inl1 << " " << _cache[i].hits << endl;
		}
	}
};

struct Stats
{
	unsigned long long l1_accesses = 0;
	unsigned long long l1_misses = 0;
	unsigned long long l2_accesses = 0;
	unsigned long long l2_misses = 0;
	unsigned long long l2_block_fills = 0;
	unsigned long long l2_evicts_at_0_hit = 0;
	unsigned long long l2_evicts_at_1_hit = 0;
	unsigned long long l2_evicts_at_2_or_more_hits = 0;

	double frac_l2_evicts_atleast_2_hits()
	{
		unsigned long long l2_evicts_atleast_1_hit = l2_evicts_at_1_hit + l2_evicts_at_2_or_more_hits;
		double l2_evicts_atleast_2_hits_frac = 0;
		if (l2_evicts_atleast_1_hit > 0)
			l2_evicts_atleast_2_hits_frac = (double)l2_evicts_at_2_or_more_hits / l2_evicts_atleast_1_hit;
		return l2_evicts_atleast_2_hits_frac;
	}

	void dumpstats(ostream *out)
	{
		*out << "L1 Accesses:              " << setw(10) << right << l1_accesses << endl;
		*out << "L1 Misses:                " << setw(10) << right << l1_misses << " "
			 << (double)l1_misses / l1_accesses << endl;
		*out << "L2 Accesses:              " << setw(10) << right << l2_accesses << endl;
		*out << "L2 Misses:                " << setw(10) << right << l2_misses << " "
			 << (double)l2_misses / l2_accesses << endl;
		*out << "L2 Block Fills:           " << setw(10) << right << l2_block_fills << endl;
		*out << "L2 Evicts at 0 Hit:       " << setw(10) << right << l2_evicts_at_0_hit << " "
			 << (double)l2_evicts_at_0_hit / l2_block_fills << endl;
		*out << "L2 Evicts atleast 2 Hits: " << setw(10) << right << l2_evicts_at_2_or_more_hits
			 << " " << frac_l2_evicts_atleast_2_hits() << endl;
	}
};

class LRUCache
{
private:
	CacheSet<L1_SET_WAYS> _l1_cache[L1_SET_SIZE];
	CacheSet<L2_SET_WAYS> _l2_cache[L2_SET_SIZE];
	timee_t _time;
	Stats _stats;

	void move_to_L1(block_t block)
	{
		CacheLine &line = _l1_cache[L1_IDX(block)].find_LRU();
		line.valid = true;
		line.block = block;
		line.time = _time;
	}

	void move_to_L2(block_t block)
	{
		_stats.l2_block_fills++;
		CacheLine &line = _l2_cache[L2_IDX(block)].find_LRU();
		if (line.valid)
		{
			if (line.hits == 0)
				_stats.l2_evicts_at_0_hit++;
			else if (line.hits == 1)
				_stats.l2_evicts_at_1_hit++;
			else
				_stats.l2_evicts_at_2_or_more_hits++;
		}
		line.valid = true;
		line.hits = 0;
		line.block = block;
		line.time = _time;
	}

	bool l1_lookup(block_t block)
	{
		_stats.l1_accesses++;
		CacheLine &line = _l1_cache[L1_IDX(block)].find(block);
		if (line.valid && line.block == block)
		{
			// L1 hit
			line.time = _time;
			CacheLine &l2_line = _l2_cache[L2_IDX(block)].find(block);
			if (l2_line.valid && l2_line.block == block)
				l2_line.time = _time;
			else
				cerr << "L2 inclusion violation";
			return true;
		}
		_stats.l1_misses++;
		return false;
	}

	bool l2_lookup(block_t block)
	{
		_stats.l2_accesses++;
		CacheLine &line = _l2_cache[L2_IDX(block)].find(block);
		if (line.valid && line.block == block)
		{
			// L2 hit
			line.time = _time;
			line.hits++;
			return true;
		}
		_stats.l2_misses++;
		return false;
	}

public:
	LRUCache()
	{
		_time = 0;
	}
	void access(unsigned block)
	{
		_time++;

		if (l1_lookup(block))
			return;

		// L1 miss
		if (l2_lookup(block))
		{
			// L2 hit
			move_to_L1(block);
			return;
		}

		// L2 miss
		move_to_L2(block);
		move_to_L1(block);
	}

	void dumpstats(ostream *out)
	{
		*out << "LRU Cache Statistics" << endl;
		_stats.dumpstats(out);
	}
};

class SRRIPCache
{
private:
	CacheSet<L1_SET_WAYS> _l1_cache[L1_SET_SIZE];
	CacheSet<L2_SET_WAYS> _l2_cache[L2_SET_SIZE];
	timee_t _time;
	Stats _stats;

	void move_to_L1(block_t block)
	{
		CacheLine &line = _l1_cache[L1_IDX(block)].find_LRU();
		if (line.valid)
		{
			block_t evict_block = line.block;
			CacheLine &l2_line = _l2_cache[L2_IDX(evict_block)].find(evict_block);
			if (l2_line.valid && l2_line.block == evict_block)
			{
				l2_line.inl1 = 0;
			}
			else
			{
				cerr << "L2 inclusion violation - SRRIP ML1" << endl;
				exit(1);
			}
		}
		line.valid = true;
		line.block = block;
		line.time = _time;
	}

	void move_to_L2(block_t block)
	{
		_stats.l2_block_fills++;
		CacheLine &line = _l2_cache[L2_IDX(block)].find_SRRIP();
		if (line.valid)
		{
			if (line.hits == 0)
				_stats.l2_evicts_at_0_hit++;
			else if (line.hits == 1)
				_stats.l2_evicts_at_1_hit++;
			else
				_stats.l2_evicts_at_2_or_more_hits++;
		}
		line.valid = true;
		line.block = block;
		line.inl1 = 1;
		line.hits = 0;
		line.rrpv = 2;
	}

	bool l1_lookup(block_t block)
	{
		_stats.l1_accesses++;
		CacheLine &line = _l1_cache[L1_IDX(block)].find(block);
		if (line.valid && line.block == block)
		{
			// L1 hit
			line.time = _time;
			CacheLine &l2_line = _l2_cache[L2_IDX(block)].find(block);
			if (l2_line.valid && l2_line.block == block)
			{
				l2_line.inl1 = 1;
			}
			else
			{
				cerr << "L2 inclusion violation - SRRIP" << endl;
				exit(1);
			}
			return true;
		}
		_stats.l1_misses++;
		return false;
	}

	bool l2_lookup(block_t block)
	{
		_stats.l2_accesses++;
		CacheLine &line = _l2_cache[L2_IDX(block)].find(block);
		if (line.valid && line.block == block)
		{
			// L2 hit
			line.hits++;
			line.rrpv = 0;
			line.inl1 = 1;
			return true;
		}
		_stats.l2_misses++;
		return false;
	}

public:
	SRRIPCache()
	{
		_time = 0;
	}
	void access(unsigned block)
	{
		_time++;

		if (l1_lookup(block))
			return;

		// L1 miss
		if (l2_lookup(block))
		{
			// L2 hit
			move_to_L1(block);
			return;
		}

		// L2 miss
		move_to_L2(block);
		move_to_L1(block);
	}

	void dumpstats(ostream *out)
	{
		*out << "SRRIP Cache Statistics" << endl;
		_stats.dumpstats(out);
	}
};

class NRUCache
{
};

#endif // !__HW4_HPP__
