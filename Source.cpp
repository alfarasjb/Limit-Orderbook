

#include <memory>
#include <vector>
#include <iostream>
#include <map>
#include <unordered_map>
#include <list>
#include <numeric> 
#include <cmath>

enum class OrderType {
	GoodTillCancel, 
	FillAndKill
};

enum class Side {
	Buy,
	Sell
};

using Price = std::int32_t; 
using Quantity = std::uint32_t;
using OrderId = std::uint64_t; 


struct LevelInfo {
	Price price_;
	Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>; 

class OrderBookLevelInfos {
private: 
	LevelInfos	bids_, asks_; 
public: 
	OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks);

	const	LevelInfos& GeBids()	const { return bids_; }
	const	LevelInfos& GetAsks()	const { return asks_;	}
	

};

//--- ORDER BOOK LEVEL INFOS
OrderBookLevelInfos::OrderBookLevelInfos(const LevelInfos &bids, const LevelInfos &asks) 
	: bids_ { bids }
	, asks_ { asks } {}

class Order {
private:
	OrderType	order_type_;
	OrderId		order_id_;
	Side		side_;
	Quantity	initial_quantity_, remaining_quantity_; 
	Price		price_;


public:
	Order(OrderType order_type, OrderId order_id, Side side, Price price, Quantity quantity);

	//--- Wrappers 
	OrderId		GetOrderId()	const { return order_id_; }
	Side		GetSide()		const { return side_;  }
	Price		GetPrice()		const { return price_;  }
	OrderType	GetOrderType()	const { return order_type_;  }
	Quantity	GetInitialQuantity() const { return initial_quantity_; }
	Quantity	GetRemainingQuantity() const { return remaining_quantity_;  }
	Quantity    GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
	bool IsFilled() const { return GetRemainingQuantity() == 0; }
	void Fill(Quantity quantity); 
	
};

//--- ORDER CLASS 
Order::Order(OrderType order_type, OrderId order_id, Side side, Price price, Quantity quantity) 
	: order_type_ { order_type }
	, order_id_ { order_id }
	, side_ { side }
	, price_ { price }
	, initial_quantity_ { quantity }
	, remaining_quantity_ { quantity} {}

void Order::Fill(Quantity quantity) {
	if (quantity > GetRemainingQuantity()) {
		throw std::logic_error(std::format("Order ({}) cannot be filled for more than remaining quantity",
			GetOrderId()));
	}
	remaining_quantity_ -= quantity; 
}


using OrderPointer = std::shared_ptr<Order>; 
using OrderPointers = std::list<OrderPointer>; 


class OrderModify {
private:
	OrderId order_id_;
	Side side_;
	Price price_;
	Quantity quantity_; 
public: 
	OrderModify(OrderId order_id, Side side, Price price, Quantity quantity); 

	OrderId GetOrderId() const { return order_id_; }
	Side GetSide() const { return side_; }
	Price GetPrice() const { return price_; }
	Quantity GetQuantity() const { return quantity_; }

	OrderPointer ToOrderPointer(OrderType type) const; 
};

OrderModify::OrderModify(OrderId order_id, Side side, Price price, Quantity quantity)
	: order_id_ { order_id }
	, side_ { side }
	, price_ { price }
	, quantity_ { quantity } {}


OrderPointer OrderModify::ToOrderPointer(OrderType type) const {
	//--- Order Needs: OrderType, OrderId, Side, Price, Quantity
	return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity()); 
}


struct TradeInfo {
	OrderId order_id_; 
	Price price_;
	Quantity quanity_;
};


class Trade {
private:
	TradeInfo bid_trade_, ask_trade_; 
public: 
	Trade(const TradeInfo& bid_trade, const TradeInfo& ask_trade); 
	
	const TradeInfo& GetBidTrade() const { return bid_trade_; }
	const TradeInfo& GetAskTrade() const { return ask_trade_; }
};

Trade::Trade(const TradeInfo& bid_trade, const TradeInfo& ask_trade) 
	: bid_trade_{ bid_trade }
	, ask_trade_{ ask_trade } {}


using Trades = std::vector<Trade>;

class OrderBook {
private:
	struct OrderEntry {
		OrderPointer order_{ nullptr };
		OrderPointers::iterator location_; 
	};
	std::map<Price, OrderPointers, std::greater<Price>> bids_; 
	std::map<Price, OrderPointers, std::less<Price>> asks_; 
	std::unordered_map<OrderId, OrderEntry> orders_; 
		 
	bool CanMatch(Side side, Price price) const; 
	Trades MatchOrders(); 

public: 
	Trades AddOrder(OrderPointer order);
	void CancelOrder(OrderId order_id); 
	Trades MatchOrder(OrderModify order); 

	std::size_t Size() const { return orders_.size(); }

	OrderBookLevelInfos GetOrderInfos() const; 
};

//--- PRIVATE 
bool OrderBook::CanMatch(Side side, Price price) const {
	if (side == Side::Buy) {
		if (asks_.empty()) return false; 

		const auto& [best_ask, _] = *asks_.begin();
		return price >= best_ask; 
	}
	else {
		if (bids_.empty()) return false;

		const auto& [best_bid, _] = *bids_.begin(); 
		return price <= best_bid; 
	}
}

Trades OrderBook::MatchOrders() {
	Trades trades; 
	trades.reserve(orders_.size());

	while (true) {
		if (bids_.empty() || asks_.empty()) break;

		auto& [bid_price, bids] = *bids_.begin(); 
		auto& [ask_price, asks] = *asks_.begin(); 

		if (bid_price < ask_price) break; 

		while (bids.size() && asks.size()) {
			auto& bid = bids.front();
			auto& ask = asks.front(); 

			Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity()); 
			bid->Fill(quantity); 
			ask->Fill(quantity); 

			if (bid->IsFilled()) {
				bids.pop_front();
				orders_.erase(bid->GetOrderId()); 
			}
			if (ask->IsFilled()) {
				asks.pop_front();
				orders_.erase(ask->GetOrderId()); 
			}
			if (bids.empty()) bids_.erase(bid_price);
			if (asks.empty()) asks_.erase(ask_price); 

			trades.push_back(Trade(
				TradeInfo(bid->GetOrderId(), bid->GetPrice(), quantity),
				TradeInfo(ask->GetOrderId(), ask->GetPrice(), quantity)
			)); 
		}
	}
	if (!bids_.empty()) {
		auto& [_, bids] = *bids_.begin(); 
		auto& order = bids.front(); 
		if (order->GetOrderType() == OrderType::GoodTillCancel) {
			//CancelOrder(order->GetOrderId()); 
		}
	}
	if (!asks_.empty()) {
		auto& [_, asks] = *asks_.begin();
		auto& order = asks.front(); 

		if (order->GetOrderType() == OrderType::FillAndKill) {
			//CancelOrder(order->GetOrderId()); 
		}
	}
	return trades; 
}

Trades OrderBook::AddOrder(OrderPointer order) {
	if (orders_.contains(order->GetOrderId())) {
		return {}; 
	}

	if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice())) {
		std::cout << "Ord Type Error" << std::endl;
		return {};
	}

	OrderPointers::iterator iterator; 
	if (order->GetSide() == Side::Buy) {
		auto& orders = bids_[order->GetPrice()]; 
		orders.push_back(order);
		iterator = std::next(orders.begin(), orders.size() - 1); 
	}
	else {
		auto& orders = asks_[order->GetPrice()]; 
		orders.push_back(order);
		iterator = std::next(orders.begin(), orders.size() - 1); 

	}
	orders_.insert({ order->GetOrderId(), OrderEntry {order, iterator} }); 
	std::cout << "Success" << std::endl; 
	return MatchOrders(); 
}

void OrderBook::CancelOrder(OrderId order_id) {
	if (!orders_.contains(order_id)) return;

	const auto& [order, iterator] = orders_.at(order_id);
	orders_.erase(order_id);

	if (order->GetSide() == Side::Sell) {
		auto price = order->GetPrice();
		auto& orders = asks_.at(price);
		orders.erase(iterator);
		if (orders.empty()) asks_.erase(price);
	}
	else {
		auto price = order->GetPrice();
		auto& orders = bids_.at(price);
		orders.erase(iterator);
		if (orders.empty()) bids_.erase(price);
	}

}

Trades OrderBook::MatchOrder(OrderModify order) {
	if (!orders_.contains(order.GetOrderId())) return { };

	const auto& [existing_order, _] = orders_.at(order.GetOrderId()); 
	CancelOrder(order.GetOrderId()); 
	return AddOrder(order.ToOrderPointer(existing_order->GetOrderType())); 
}

OrderBookLevelInfos OrderBook::GetOrderInfos() const {
	LevelInfos bid_infos, ask_infos; 
	bid_infos.reserve(orders_.size()); 
	ask_infos.reserve(orders_.size()); 
	
	auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
		return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
			[](Quantity running_sum, const OrderPointer& order)
			{ return running_sum + order->GetRemainingQuantity(); }) };
		};

	for (const auto& [price, orders] : bids_) bid_infos.push_back(CreateLevelInfos(price, orders));
	for (const auto& [price, orders] : asks_) ask_infos.push_back(CreateLevelInfos(price, orders));

	return OrderBookLevelInfos{ bid_infos, ask_infos };
}


int main() {
	OrderBook orderbook;
	const OrderId order_id = 1;
	orderbook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, order_id, Side::Sell, 100, 10));
	std::cout << orderbook.Size() << std::endl;
	//orderbook.CancelOrder(order_id);
	//std::cout << orderbook.Size() << std::endl; 
	return 0;
}