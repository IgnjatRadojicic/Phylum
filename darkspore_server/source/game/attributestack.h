
#ifndef _GAME_ATTRIBUTESTACK_HEADER
#define _GAME_ATTRIBUTESTACK_HEADER

// Include
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

// Game
namespace Game {
	// AttributeOp
	// The values of the game's nAttributeOp table.
	enum class AttributeOp : uint8_t {
		Add = 1,
		PercentChange,
		Clamp,
		AddBest,
		PercentChangeBest
	};

	// AttributeStack
	// Holds the changes scripts make to one object's attributes and works out the final values.
	// The game's files name the operations but not their order, so the order is approximate:
	// (base + adds + best add) * (1 + percents + best percent), then the lowest clamp.
	class AttributeStack {
		public:
			// The current value becomes the base when an attribute has no other changes.
			uint32_t Add(uint8_t attribute, float value, AttributeOp op, float currentValue);

			// Returns the attribute that changed, or nothing if the handle is unknown.
			std::optional<uint8_t> Remove(uint32_t handle);

			bool Has(uint8_t attribute) const;

			void SetBase(uint8_t attribute, float value);
			float GetBase(uint8_t attribute) const;

			float Resolve(uint8_t attribute) const;

		private:
			struct Change {
				uint32_t handle = 0;
				float value = 0;
				AttributeOp op = AttributeOp::Add;
			};

			struct Entry {
				float base = 0;
				std::vector<Change> changes;
			};

			std::map<uint8_t, Entry> mEntries;
			uint32_t mNextHandle = 1;
	};

	// Script attribute numbers keep the game's original order. AttributeType leaves out the ones
	// the client does not see, so numbers from 74 upwards differ.
	uint8_t AttributeFromScript(uint8_t scriptAttribute);
}

#endif
