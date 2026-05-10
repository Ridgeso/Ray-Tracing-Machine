#pragma once

namespace common
{

	template <typename Derive>
	struct Trait
	{
		const Derive& getImpl() const
		{
			return static_cast<const Derive&>(*this);
		}

		Derive& getImpl()
		{
			return static_cast<Derive&>(*this);
		}
	};

}
