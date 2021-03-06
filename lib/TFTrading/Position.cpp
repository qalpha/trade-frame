/************************************************************************
 * Copyright(c) 2010, One Unified. All rights reserved.                 *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

#include "stdafx.h"

#include "OrderManager.h"

#include "Position.h"

namespace ou { // One Unified
namespace tf { // TradeFrame

Position::Position( pInstrument_cref pInstrument, pProvider_t pExecutionProvider, pProvider_t pDataProvider,
  const idAccount_t& idExecutionAccount, const idAccount_t& idDataAccount, 
  const idPortfolio_t& idPortfolio, const std::string& sName, const std::string& sAlgorithm ) 
: m_pExecutionProvider( pExecutionProvider ), m_pDataProvider( pDataProvider ), 
  m_pInstrument( pInstrument ), 
  m_dblMultiplier( 1 ), m_bConnectedToDataProvider( false ),
  m_bInstrumentAssigned ( true ), m_bExecutionAccountAssigned( true ), m_bDataAccountAssigned( true ),
  m_row( idPortfolio, sName, pInstrument->GetInstrumentName(), idExecutionAccount, idDataAccount, sAlgorithm )
{
  Construction();
}

Position::Position( pInstrument_cref pInstrument, pProvider_t pExecutionProvider, pProvider_t pDataProvider ) 
: m_pExecutionProvider( pExecutionProvider ), m_pDataProvider( pDataProvider ), 
  m_pInstrument( pInstrument ), 
  m_dblMultiplier( 1 ), m_bConnectedToDataProvider( false ),
  m_bInstrumentAssigned ( true ), m_bExecutionAccountAssigned( true ), m_bDataAccountAssigned( true )
{
  Construction();
}

Position::Position( 
  pInstrument_cref pInstrument, pProvider_t pExecutionProvider, pProvider_t pDataProvider, const std::string& sNotes ) 
: m_pExecutionProvider( pExecutionProvider ), m_pDataProvider( pDataProvider ), 
  m_pInstrument( pInstrument ), 
  m_dblMultiplier( 1 ), m_bConnectedToDataProvider( false ),
  m_bInstrumentAssigned ( true ), m_bExecutionAccountAssigned( true ), m_bDataAccountAssigned( true )
{
  m_row.sNotes = sNotes;
  Construction();
}

Position::Position( 
  pInstrument_cref pInstrument, pProvider_t pExecutionProvider, pProvider_t pDataProvider, const TableRowDef& row ) 
: m_row( row ),
  m_pExecutionProvider( pExecutionProvider ), m_pDataProvider( pDataProvider ), 
  m_pInstrument( pInstrument ), 
  m_dblMultiplier( 1 ), m_bConnectedToDataProvider( false ),
  m_bInstrumentAssigned ( true ), m_bExecutionAccountAssigned( true ), m_bDataAccountAssigned( true )
{
  Construction();
}

Position::Position( const TableRowDef& row ) 
: m_row( row ),
  m_dblMultiplier( 1 ), m_bConnectedToDataProvider( false ),
  m_bInstrumentAssigned ( false ), m_bExecutionAccountAssigned( false ), m_bDataAccountAssigned( false )
{
  // need flags to wait for execution, data, instrument variables to be set
}

Position::Position( void ) 
: m_dblMultiplier( 1 ), m_bConnectedToDataProvider( false ),
  m_bInstrumentAssigned ( false ), m_bExecutionAccountAssigned( false ), m_bDataAccountAssigned( false )
{
  // need flags to wait for execution, data, instrument variables to be set
}

void Position::Construction( void ) {
  m_dblMultiplier = m_pInstrument->GetMultiplier();
  if ( m_pDataProvider->ProvidesQuotes() ) {
    m_pDataProvider->AddQuoteHandler( m_pInstrument, MakeDelegate( this, &Position::HandleQuote ) );
  }
  if ( m_pDataProvider->ProvidesTrades() ) {
    m_pDataProvider->AddTradeHandler( m_pInstrument, MakeDelegate( this, &Position::HandleTrade ) );
  }
  m_pDataProvider->OnDisconnecting.Add( MakeDelegate( this, &Position::DisconnectFromDataProvider ) );
  m_bConnectedToDataProvider = true;
}

void Position::Set( pInstrument_cref pInstrument, pProvider_t& pExecutionProvider, pProvider_t& pDataProvider ) {

  m_pInstrument = pInstrument;
  m_bInstrumentAssigned = true;

  m_pExecutionProvider = pExecutionProvider;
  m_bExecutionAccountAssigned = true;

  m_pDataProvider = pDataProvider;
  m_bDataAccountAssigned = true;

  Construction();

}

Position::~Position(void) {
  if ( m_bConnectedToDataProvider ) {
    DisconnectFromDataProvider( 0 );
  }
  for ( std::vector<pOrder_t>::iterator iter = m_vAllOrders.begin(); iter != m_vAllOrders.end(); ++iter ) {
    iter->get()->OnCommission.Remove( MakeDelegate( this, &Position::HandleCommission ) );
    iter->get()->OnExecution.Remove( MakeDelegate( this, &Position::HandleExecution ) );
  }
  m_vOpenOrders.clear();
  m_vClosedOrders.clear();
  m_vAllOrders.clear();
}

void Position::DisconnectFromDataProvider( int ) {
  m_pDataProvider->OnDisconnecting.Remove( MakeDelegate( this, &Position::DisconnectFromDataProvider ) );
  if ( m_pDataProvider->ProvidesQuotes() ) {
    m_pDataProvider->RemoveQuoteHandler( m_pInstrument, MakeDelegate( this, &Position::HandleQuote ) );
  }
  if ( m_pDataProvider->ProvidesTrades() ) {
    m_pDataProvider->RemoveTradeHandler( m_pInstrument, MakeDelegate( this, &Position::HandleTrade ) );
  }
  m_bConnectedToDataProvider = false;
}

void Position::HandleQuote( quote_t quote ) {

  if ( ( 0 == quote.Ask() ) || ( 0 == quote.Bid() ) ) return;

  OnQuote( quote );

  double dblPreviousUnRealizedPL = m_row.dblUnRealizedPL;

  bool bProcessed(false);
  double dblMarketValue;
  switch ( m_row.eOrderSideActive ) {
    case OrderSide::Buy:
      dblMarketValue = m_row.nPositionActive * quote.Bid() * m_dblMultiplier;
      m_row.dblUnRealizedPL = dblMarketValue - m_row.dblConstructedValue;
      bProcessed = true;
      break;
    case OrderSide::Sell:
      dblMarketValue = - ( m_row.nPositionActive * quote.Ask() ) * m_dblMultiplier;
      m_row.dblUnRealizedPL = dblMarketValue - m_row.dblConstructedValue;
      bProcessed = true;
      break;
  }

  if ( bProcessed ) {
    OnQuotePostProcess( quote_pair_t( *this, quote ) );
    if ( dblPreviousUnRealizedPL != m_row.dblUnRealizedPL ) {
      OnUnRealizedPL( PositionDelta_delegate_t( *this, dblPreviousUnRealizedPL, m_row.dblUnRealizedPL ) );
    }
  }

}

void Position::HandleTrade( trade_t trade ) {
  OnTrade( trade );
}

Order::pOrder_t Position::PlaceOrder( // market
  OrderType::enumOrderType eOrderType,
  OrderSide::enumOrderSide eOrderSide,
  boost::uint32_t nOrderQuantity
) {
  pOrder_t pOrder = ConstructOrder( eOrderType, eOrderSide, nOrderQuantity );
  PlaceOrder( pOrder );
  return pOrder;
}

Order::pOrder_t Position::PlaceOrder( // limit or stop
  OrderType::enumOrderType eOrderType,
  OrderSide::enumOrderSide eOrderSide,
  boost::uint32_t nOrderQuantity,
  double dblPrice1
) {
  pOrder_t pOrder = ConstructOrder( eOrderType, eOrderSide, nOrderQuantity, dblPrice1 );
  PlaceOrder( pOrder );
  return pOrder;
}

Order::pOrder_t Position::PlaceOrder( // limit and stop
  OrderType::enumOrderType eOrderType,
  OrderSide::enumOrderSide eOrderSide,
  boost::uint32_t nOrderQuantity,
  double dblPrice1,  
  double dblPrice2
) {
  pOrder_t pOrder = ConstructOrder( eOrderType, eOrderSide, nOrderQuantity, dblPrice1, dblPrice2 );
  PlaceOrder( pOrder );
  return pOrder;
}

Order::pOrder_t Position::ConstructOrder( // market
  OrderType::enumOrderType eOrderType,
  OrderSide::enumOrderSide eOrderSide,
  boost::uint32_t nOrderQuantity
) {
  assert( OrderSide::Unknown != eOrderSide );
  assert( OrderType::Market == eOrderType );
  //pOrder_t pOrder( new Order( m_pInstrument, eOrderType, eOrderSide, nOrderQuantity, m_row.idPosition ) );
  pOrder_t pOrder
   = OrderManager::LocalCommonInstance().ConstructOrder( m_pInstrument, eOrderType, eOrderSide, nOrderQuantity, m_row.idPosition );
  //PlaceOrder( pOrder );
  return pOrder;
}

Order::pOrder_t Position::ConstructOrder( // limit or stop
  OrderType::enumOrderType eOrderType,
  OrderSide::enumOrderSide eOrderSide,
  boost::uint32_t nOrderQuantity,
  double dblPrice1
) {
  assert( OrderSide::Unknown != eOrderSide );
  assert( ( OrderType::Limit == eOrderType) || ( OrderType::Stop == eOrderType ) || ( OrderType::Trail == eOrderType ) );
  //pOrder_t pOrder( new Order( m_pInstrument, eOrderType, eOrderSide, nOrderQuantity, dblPrice1, m_row.idPosition ) );
  pOrder_t pOrder
   = OrderManager::LocalCommonInstance().ConstructOrder( m_pInstrument, eOrderType, eOrderSide, nOrderQuantity, dblPrice1, m_row.idPosition );
  //PlaceOrder( pOrder );
  return pOrder;
}

Order::pOrder_t Position::ConstructOrder( // limit and stop
  OrderType::enumOrderType eOrderType,
  OrderSide::enumOrderSide eOrderSide,
  boost::uint32_t nOrderQuantity,
  double dblPrice1,  
  double dblPrice2
) {
  assert( OrderSide::Unknown != eOrderSide );
  assert( ( OrderType::StopLimit == eOrderType) || ( OrderType::TrailLimit == eOrderType ) );
  //pOrder_t pOrder( new Order( m_pInstrument, eOrderType, eOrderSide, nOrderQuantity, dblPrice1, dblPrice2, m_row.idPosition ) );
  pOrder_t pOrder
   = OrderManager::LocalCommonInstance().ConstructOrder( m_pInstrument, eOrderType, eOrderSide, nOrderQuantity, dblPrice1, dblPrice2, m_row.idPosition );
  //PlaceOrder( pOrder );
  return pOrder;
}

void Position::PlaceOrder( pOrder_t pOrder ) {

//  if ( OrderSide::Unknown != m_row.eOrderSidePending ) { // ensure new order matches existing orders
//    if ( ( m_row.eOrderSidePending != pOrder->GetOrderSide() ) && ( OrderType::Market == pOrder->GetOrderType() ) ) {  // check only for market orders, not limit orders?
//      throw std::runtime_error( "Position::PlaceOrder, new order does not match pending order type" );
//    }
//  }

  if ( 0 == m_row.nPositionPending ) m_row.eOrderSidePending = pOrder->GetOrderSide();  // first to set non-zero gives us our predominant side

  m_row.nPositionPending += pOrder->GetQuantity();
  m_vAllOrders.push_back( pOrder );
  m_vOpenOrders.push_back( pOrder );
  pOrder->OnExecution.Add( MakeDelegate( this, &Position::HandleExecution ) ); 
  pOrder->OnCommission.Add( MakeDelegate( this, &Position::HandleCommission ) );
  pOrder->OnOrderCancelled.Add( MakeDelegate( this, &Position::HandleCancellation ) );
  OrderManager::LocalCommonInstance().PlaceOrder( &(*m_pExecutionProvider), pOrder );

  OnPositionChanged( *this );
}

void Position::CancelOrders( void ) {
  // may have a problem getting out of sync with broker if orders are cancelled by broker
  // todo:  on power up, need to relink these active orders back in with the position
  for ( std::vector<pOrder_t>::iterator iter = m_vOpenOrders.begin(); iter != m_vOpenOrders.end(); ++iter ) {
    CancelOrder( iter );  // this won't work as the iterator is invalidated with each order removal
  }
  //m_OpenOrders.clear();
}

void Position::CancelOrder( idOrder_t idOrder ) {
  for ( vOrders_t::iterator iter = m_vOpenOrders.begin(); iter != m_vOpenOrders.end(); ++iter ) {
    if ( idOrder == iter->get()->GetOrderId() ) {
      CancelOrder( iter );
      //m_OpenOrders.erase( iter );
      break;
    }
  }
}

void Position::HandleCancellation( const Order& order ) {
  Order::idOrder_t idOrder = order.GetOrderId();
  for ( vOrders_t::iterator iter = m_vOpenOrders.begin(); iter != m_vOpenOrders.end(); ++iter ) {
    if ( idOrder == iter->get()->GetOrderId() ) {
      if ( m_row.nPositionPending >= iter->get()->GetQuanRemaining() ) {
        m_row.nPositionPending -= iter->get()->GetQuanRemaining(); 
        if ( 0 == m_row.nPositionPending ) m_row.eOrderSidePending = OrderSide::Unknown;
        //CancelOrder( iter );
        m_vClosedOrders.push_back( *iter );
        m_vOpenOrders.erase( iter );
        break;
      }
      else {
        throw std::runtime_error( "problems" );
      }
    }
  }
  OnPositionChanged( *this );
}

void Position::CancelOrder( vOrders_iter_t iter ) {
  OrderManager::LocalCommonInstance().CancelOrder( iter->get()->GetOrderId() );
}

void Position::ClosePosition( OrderType::enumOrderType eOrderType ) {
  // should outstanding orders be auto cancelled?
  // position is closed with a market order, can try to do limit in the future, but need active market data
  if ( 0 != m_row.nPositionActive ) {
    switch ( m_row.eOrderSideActive ) {
      case OrderSide::Buy:
        PlaceOrder( eOrderType, OrderSide::Sell, m_row.nPositionActive );
        break;
      case OrderSide::Sell:
        PlaceOrder( eOrderType, OrderSide::Buy, m_row.nPositionActive );
        break;
      case OrderSide::Unknown:
        break;
    }
  }
}

void Position::UpdateRowValues( double price, boost::uint32_t quan, OrderSide::enumOrderSide side ) {

  // can only be called by HandleExecution as HandleExecution uses the before and after values of RealizedPL

  double dblAvgConstructedCost = 0;
  double dblRealizedPL = 0;
  bool bTwoStep = false;
  boost::uint32_t nRemaining( 0 );

  switch ( m_row.eOrderSideActive ) {
    case OrderSide::Buy:  // existing is long
      switch ( side ) {
        case OrderSide::Buy:  // increase long
          m_row.nPositionActive += quan;
          m_row.dblConstructedValue += quan * price * m_dblMultiplier;
          break;
        case OrderSide::Sell:  // decrease long
          if ( quan > m_row.nPositionActive ) {
            bTwoStep = true;
            nRemaining = quan - m_row.nPositionActive;
            quan = m_row.nPositionActive;
          }
          dblAvgConstructedCost = m_row.dblConstructedValue / ( m_row.nPositionActive * m_dblMultiplier );
          dblRealizedPL = quan * ( price - dblAvgConstructedCost ) * m_dblMultiplier;
          m_row.dblRealizedPL += dblRealizedPL;
          m_row.nPositionActive -= quan;
          m_row.dblConstructedValue -= quan * dblAvgConstructedCost * m_dblMultiplier;
          //m_dblConstructedValue -= ( exec.GetSize() * exec.GetPrice() * m_dblMultiplier - dblRealizedPL );
          if ( 0 == m_row.nPositionActive ) {
            m_row.eOrderSideActive = OrderSide::Unknown;
            m_row.dblUnRealizedPL = 0.0;
            dblAvgConstructedCost = 0.0;
          }
          if ( bTwoStep ) {
            UpdateRowValues( price, nRemaining, side );
          }
          break;
      }
      break;
    case OrderSide::Sell:  // existing is short
      switch ( side ) {
        case OrderSide::Sell:  // increase short
          m_row.nPositionActive += quan;
          m_row.dblConstructedValue -= quan * price * m_dblMultiplier;
          break;
        case OrderSide::Buy:  // decrease short
          if ( quan > m_row.nPositionActive ) {
            bTwoStep = true;
            nRemaining = quan - m_row.nPositionActive;
            quan = m_row.nPositionActive;
          }
          dblAvgConstructedCost = m_row.dblConstructedValue / ( m_row.nPositionActive * m_dblMultiplier );
          dblRealizedPL = quan * ( - price - dblAvgConstructedCost ) * m_dblMultiplier;
          m_row.dblRealizedPL += dblRealizedPL;
          m_row.nPositionActive -= quan;
          m_row.dblConstructedValue -= quan * dblAvgConstructedCost * m_dblMultiplier;
          //m_dblConstructedValue += ( exec.GetSize() * exec.GetPrice() * m_dblMultiplier + dblRealizedPL );  // is this correctly calculated?
          if ( 0 == m_row.nPositionActive ) {
            m_row.eOrderSideActive = OrderSide::Unknown;
            m_row.dblUnRealizedPL = 0.0;
            dblAvgConstructedCost = 0.0;
          }
          if ( bTwoStep ) {
            UpdateRowValues( price, nRemaining, side );
          }
          break;
      }
      break;
    case OrderSide::Unknown:  // no active position, so start here
      assert( 0 == m_row.nPositionActive );
      m_row.eOrderSideActive = side;
      m_row.nPositionActive = quan;
      switch ( m_row.eOrderSideActive ) {
        case OrderSide::Buy:
          m_row.dblConstructedValue += quan * price * m_dblMultiplier;
          break;
        case OrderSide::Sell:
          m_row.dblConstructedValue -= quan * price * m_dblMultiplier;
          break;
      }
      break;
  }

}

// before entry to this method, sanity check:  side on execution is same as side on order
void Position::HandleExecution( const std::pair<const Order&, const Execution&>& status ) {

  // should be able to calculate profit/loss & position cost as exections are encountered
  // should be able to calculate position cost basis as position is updated (with and without commissions)
  // will need market feed in order to calculate profit/loss  -- handled in the OnQuote method

  const Order& order = status.first;
  const Execution& exec = status.second;
  Order::idOrder_t orderId = order.GetOrderId();

  double dblOldRealizedPL = m_row.dblRealizedPL;
  double dblOldUnRealizedPL = m_row.dblUnRealizedPL;

  //std::cout << "Position Exec: " << exec.GetSize() << "," << exec.GetPrice() << std::endl;

  // update position, regardless of whether we see order open or closed
  UpdateRowValues( exec.GetPrice(), exec.GetSize(), exec.GetOrderSide() );

  if ( ( 0 == m_row.nPositionActive ) && ( OrderSide::Unknown != m_row.eOrderSideActive ) ) {
    std::cout << "problems" << std::endl;
  }
  
  // check that we think that the order is still active
  bool bOrderFound = false;
  for ( std::vector<pOrder_t>::iterator iter = m_vOpenOrders.begin(); iter != m_vOpenOrders.end(); ++iter ) {
    if ( orderId == iter->get()->GetOrderId() ) {
      // update position based upon current position and what is executing
      //   decrease position when execution is opposite position
      //   increase position when execution is same as position
      m_row.nPositionPending -= exec.GetSize();
      if ( 0 == m_row.nPositionPending ) m_row.eOrderSidePending = OrderSide::Unknown;

      if ( 0 == order.GetQuanRemaining() ) {  // move from open to closed on order filled
        m_vClosedOrders.push_back( *iter );
        m_vOpenOrders.erase( iter );
      }
      
      bOrderFound = true;
      break;
    }
  }
  if ( !bOrderFound ) {
    // need to handle the case where order was cancelled, but not in time to prevent execution
    throw std::runtime_error( "Position::HandleExecution doesn't have an Open Order" );
  }

  OnUnRealizedPL( PositionDelta_delegate_t( *this, dblOldUnRealizedPL, m_row.dblUnRealizedPL ) );  // used by portfolio updates

  OnExecutionRaw( execution_pair_t( *this, exec ) );
  OnUpdateExecutionForPortfolioManager( *this );

  OnExecution( PositionDelta_delegate_t( *this, dblOldRealizedPL, m_row.dblRealizedPL ) );  // used by portfolio updates

  OnPositionChanged( *this );
  
}

void Position::HandleCommission( const Order& order ) {
  //std::cout << "Position Comm: " << m_row.dblCommissionPaid << "," << order.GetCommission();

  double dblNewCommissionPaid = order.GetIncrementalCommission();
  //std::cout << "," << dblNewCommissionPaid << std::endl;

  if ( 0 != dblNewCommissionPaid ) {
    m_row.dblCommissionPaid += dblNewCommissionPaid;
    OnUpdateCommissionForPortfolioManager( *this );
    OnCommission( PositionDelta_delegate_t( *this, 0, dblNewCommissionPaid ) );
  }
}

std::ostream& operator<<( std::ostream& os, const Position& position ) {
  os 
    << "Position " << position.m_pInstrument->GetInstrumentName() << ": "
    << "Active " << position.m_row.nPositionActive
    << ", unRPL " << position.m_row.dblUnRealizedPL 
    << ", RPL " << position.m_row.dblRealizedPL
    << ", Cmsn " << position.m_row.dblCommissionPaid
    << ", PL-C " << position.m_row.dblRealizedPL - position.m_row.dblCommissionPaid
    ;
  return os;
}

// process execution to convert Pending to Active

} // namespace tf
} // namespace ou
