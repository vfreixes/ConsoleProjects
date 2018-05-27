#pragma once

#include <atomic>
#include <limits>

namespace Utilities
{
	template<typename T, int Capacity>
	class LockfreeStack
	{
		static_assert(Capacity > 0, "Capacity must be positive");
		static_assert(Capacity <= std::numeric_limits<short>::max() + 1, "Indexes have to be representable with a short");

		struct Node
		{
			T value;
			short next;
		};

		struct alignas(int)Head // TODO align to cache line size. 64B?
		{
			unsigned short aba;
			short node;
		};

		std::atomic<Head> head, free;
		std::atomic_int size;

		Node allocatedNodes[Capacity];


		short Pop(std::atomic<Head> &head)
		{
			Head next, orig = head.load();
			do {
				if (orig.node < 0)
					return -1;  // empty stack
				next.aba = orig.aba + 1;
				next.node = allocatedNodes[orig.node].next;
			} while (!head.compare_exchange_weak(orig, next));
			return orig.node;
		}

		void Push(std::atomic<Head> &head, Node *node)
		{
			Head next, orig = head.load();
			do {
				node->next = orig.node;
				next.aba = orig.aba + 1;
				next.node = static_cast<short>(node - &allocatedNodes[0]);
				assert(next.node >= 0);
				assert(next.node < Capacity);
			} while (!head.compare_exchange_weak(orig, next));
		}

	public:
		LockfreeStack()
			: head(Head{ 0, -1 })
			, free(Head{ 0, 0 })
			, size(0)
		{
			assert(head.is_lock_free());
			assert(free.is_lock_free());

			for (int i = 0; i < Capacity - 1; i++)
				allocatedNodes[i].next = (short)(i + 1);
			allocatedNodes[Capacity - 1].next = -1;
		}

		void Push(const T &value)
		{
			short nodeIndex = Pop(free);
			assert(nodeIndex >= 0);
			Node *node = &allocatedNodes[nodeIndex];
			node->value = value;
			Push(head, node);
			size++;
		}

		T Pop()
		{
			short nodeIndex = Pop(head);
			assert(nodeIndex >= 0);
			Node *node = &allocatedNodes[nodeIndex];
			--size;
			T value = node->value;
			Push(free, node);
			return value;
		}
	};


	template<typename T, int Capacity>
	class SpinlockQueue
	{
		static_assert(Capacity > 0, "Capacity must be positive");
		static_assert(Capacity <= std::numeric_limits<short>::max() + 1, "Indexes have to be representable with a short");

		struct Node
		{
			T payload;
			short next;
		};

		short adder, remover;
		short free;

		std::atomic_bool lock;

		Node nodes[Capacity];


	public:

		SpinlockQueue()
			: adder(-1)
			, remover(-1)
			, free(0)
			, lock(false)
		{
			for (int i = 0; i < Capacity - 1; i++)
			{
				nodes[i].next = (short)(i + 1);
			}
			nodes[Capacity - 1].next = -1;
		}

		bool Add(const T &value)
		{
			bool lockAquired = false;
			while (!lock.compare_exchange_weak(lockAquired, true)) lockAquired = false;


			if (free < 0)
			{
				lock.store(false);
				return false;
			}
			short nodeIndex = free;
			free = nodes[free].next;

			nodes[nodeIndex] = Node{ value, -1 };

			if (adder < 0)
			{
				adder = remover = nodeIndex;
			}
			else
			{
				assert(nodes[adder].next == -1);
				adder = nodes[adder].next = nodeIndex;
			}

			lock.store(false);

			return true;
		}

		bool Remove(T &value)
		{
			bool lockAquired = false;
			while (!lock.compare_exchange_weak(lockAquired, true)) lockAquired = false;
			
			if (remover < 0)
			{
				lock.store(false);
				return false;
			}

			short nodeIndex = remover;
			value = nodes[nodeIndex].payload;
			remover = nodes[nodeIndex].next;

			if (remover < 0)
				adder = -1;

			nodes[nodeIndex] = Node{ T{}, free  };
			free = nodeIndex;
			assert(free >= 0);
			assert(free < Capacity);

			lock.store(false);
			return true;
		}
	};
}
