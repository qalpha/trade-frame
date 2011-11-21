/************************************************************************
 * Copyright(c) 2011, One Unified. All rights reserved.                 *
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

#pragma once

#include <map>

#include <TFTrading/Position.h>

class OrdersOutstanding {

public:
  typedef ou::tf::COrder::idOrder_t idOrder_t;
  typedef ou::tf::CPosition::pPosition_t pPosition_t;
  typedef ou::tf::CPosition::pOrder_t pOrder_t;

protected:

  struct structOrderMatching {
    double dblBasis;
    pOrder_t pOrderBasis;
    pOrder_t pOrderClosing;
    structOrderMatching( void ) : dblBasis( 0 ) {};
    structOrderMatching( double dblBasis_, pOrder_t pOrderBasis_, pOrder_t pOrderClosing_ ) 
      : dblBasis( dblBasis_ ), pOrderBasis( pOrderBasis_ ), pOrderClosing( pOrderClosing_ ) {};
    structOrderMatching( double dblBasis_, pOrder_t pOrderBasis_ ) 
      : dblBasis( dblBasis_ ), pOrderBasis( pOrderBasis_ ) {};
  };

  typedef std::multimap<double, structOrderMatching> mapOrders_t;
  typedef std::pair<double, structOrderMatching> mapOrders_pair_t;
  typedef mapOrders_t::iterator mapOrders_iter_t;
  mapOrders_t m_mapOrdersToMatch;

public:
  OrdersOutstanding( pPosition_t pPosition );
  virtual ~OrdersOutstanding( void ) {};
  void AddOrderFilling( pOrder_t pOrder );  // base order we need to match with closing order
  void CancelAll( void );

  // should be protected but doesn't work there
  void HandleMatchingOrderFilled( const ou::tf::COrder& order );
  void HandleMatchingOrderCancelled( const ou::tf::COrder& order );

  unsigned int GetCountOfOutstandingMatches( void ) { return m_mapOrdersToMatch.size(); };

protected:

  pPosition_t m_pPosition;

  typedef std::map<idOrder_t, pOrder_t> mapOrdersFilling_t;
  mapOrdersFilling_t m_mapBaseOrdersFilling;
  
private:
  void HandleBaseOrderFilled( const ou::tf::COrder& order );
};

class OrdersOutstandingLongs: public OrdersOutstanding {
public:
  typedef ou::tf::CPosition::pPosition_t pPosition_t;
  typedef ou::tf::CPosition::pOrder_t pOrder_t;
  OrdersOutstandingLongs( pPosition_t pPosition ): OrdersOutstanding( pPosition ) {};
  ~OrdersOutstandingLongs( void ) {};
  void HandleQuote( const ou::tf::CQuote& quote );  // set from external
protected:
private:
};

class OrdersOutstandingShorts: public OrdersOutstanding {
public:
  typedef ou::tf::CPosition::pPosition_t pPosition_t;
  typedef ou::tf::CPosition::pOrder_t pOrder_t;
  OrdersOutstandingShorts( pPosition_t pPosition ): OrdersOutstanding( pPosition ) {};
  ~OrdersOutstandingShorts( void ) {};
  void HandleQuote( const ou::tf::CQuote& quote );  // set from external
protected:
private:
};

