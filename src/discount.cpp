// -*- mode: C++; c-indent-level: 4; c-basic-offset: 4; indent-tabs-mode: nil; -*-
//
//  RQuantLib function DiscountCurve
//
//  Copyright (C) 2005 - 2007  Dominick Samperi
//  Copyright (C) 2007 - 2009  Dirk Eddelbuettel 
//  Copyright (C) 2009 - 2011  Dirk Eddelbuettel and Khanh Nguyen
//  Copyright (C) 2012 - 2014  Dirk Eddelbuettel
//
//  This file is part of RQuantLib.
//
//  RQuantLib is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 2 of the License, or
//  (at your option) any later version.
//
//  RQuantLib is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with RQuantLib.  If not, see <http://www.gnu.org/licenses/>.

#include "rquantlib.h"

// [[Rcpp::export]]
Rcpp::List discountCurveEngine(Rcpp::List rparams, 
                               Rcpp::List tslist, 
                               Rcpp::NumericVector times) {

    std::vector<std::string> tsNames = tslist.names();

    int i;
    QuantLib::Date todaysDate(Rcpp::as<QuantLib::Date>(rparams["tradeDate"])); 
    QuantLib::Date settlementDate(Rcpp::as<QuantLib::Date>(rparams["settleDate"]));
    //std::cout << "TradeDate: " << todaysDate << std::endl << "Settle: " << settlementDate << std::endl;

    RQLContext::instance().settleDate = settlementDate;
    QuantLib::Date evalDate = QuantLib::Settings::instance().evaluationDate();
    QuantLib::Settings::instance().evaluationDate() = todaysDate;
    std::string firstQuoteName = tsNames[0];
    
    double dt = Rcpp::as<double>(rparams["dt"]);
    
    std::string interpWhat, interpHow;
    bool flatQuotes = true;
    if (firstQuoteName.compare("flat") != 0) {
        // Get interpolation method (not needed for "flat" case)
        interpWhat = Rcpp::as<std::string>(rparams["interpWhat"]);
        interpHow  = Rcpp::as<std::string>(rparams["interpHow"]);
        flatQuotes = false;
    }

    // initialise from the singleton instance
    QuantLib::Calendar calendar = RQLContext::instance().calendar;
    //Integer fixingDays = RQLContext::instance().fixingDays;

    // Any DayCounter would be fine.
    // ActualActual::ISDA ensures that 30 years is 30.0
    QuantLib::DayCounter termStructureDayCounter = QuantLib::ActualActual(QuantLib::ActualActual::ISDA);
    double tolerance = 1.0e-8;

    boost::shared_ptr<QuantLib::YieldTermStructure> curve;

    if (firstQuoteName.compare("flat") == 0) {            // Create a flat term structure.
        double rateQuote = Rcpp::as<double>(tslist[0]);
        //boost::shared_ptr<Quote> flatRate(new SimpleQuote(rateQuote));
        //boost::shared_ptr<FlatForward> ts(new FlatForward(settlementDate,
        //			      Handle<Quote>(flatRate),
        //			      ActualActual()));
        boost::shared_ptr<QuantLib::SimpleQuote> rRate(new QuantLib::SimpleQuote(rateQuote));
        curve = flatRate(settlementDate,rRate,QuantLib::ActualActual());

    } else {             // Build curve based on a set of observed rates and/or prices.
        std::vector<boost::shared_ptr<QuantLib::RateHelper> > curveInput;
        for(i = 0; i < tslist.size(); i++) {
            std::string name = tsNames[i];
            double val = Rcpp::as<double>(tslist[i]);
            boost::shared_ptr<QuantLib::RateHelper> rh = ObservableDB::instance().getRateHelper(name, val);
            // edd 2009-11-01 FIXME NULL_RateHelper no longer builds under 0.9.9
            // if (rh == NULL_RateHelper)
            if (rh.get() == NULL)
                throw std::range_error("Unknown rate in getRateHelper");
            curveInput.push_back(rh);
        }
        boost::shared_ptr<QuantLib::YieldTermStructure> 
            ts = getTermStructure(interpWhat, interpHow, settlementDate, 
                                  curveInput, termStructureDayCounter, tolerance);
        curve = ts;
    }

    // Return discount, forward rate, and zero coupon curves
    //int numCol = 2;
    //std::vector<std::string> colNames(numCol);
    //colNames[0] = "date";
    //colNames[1] = "zeroRates";
    //RcppFrame frame(colNames);
        
    int ntimes = times.size(); //Rf_length(times);
    //SEXP disc  = PROTECT(Rf_allocVector(REALSXP, ntimes));
    //SEXP fwds  = PROTECT(Rf_allocVector(REALSXP, ntimes));
    //SEXP zero  = PROTECT(Rf_allocVector(REALSXP, ntimes));
    Rcpp::NumericVector disc(ntimes), fwds(ntimes), zero(ntimes);

    QuantLib::Date current = settlementDate;
    for (i = 0; i < ntimes; i++) {          
        //t = REAL(times)[i];                                                    
        //REAL(disc)[i] = curve->discount(t);
        //REAL(fwds)[i] = curve->forwardRate(t, t+dt, Continuous);
        //REAL(zero)[i] = curve->zeroRate(t, Continuous);
        double t = times[i];
        disc[i] = curve->discount(t);
        fwds[i] = curve->forwardRate(t, t+dt, QuantLib::Continuous);
        zero[i] = curve->zeroRate(t, QuantLib::Continuous);
    }

    int n = curve->maxDate() - settlementDate;
    //std::cout << "MaxDate " << curve->maxDate() << std::endl;
    //std::cout << "Settle " << settlementDate << std::endl;
    //n = std::min(300, n);

    QuantLib::Settings::instance().evaluationDate() = evalDate;

    Rcpp::DateVector dates(n);
    Rcpp::NumericVector zeroRates(n);
    QuantLib::Date d = current; 
    for (int i = 0; i<n && d < curve->maxDate(); i++){
        dates[i] = Rcpp::Date(d.month(), d.dayOfMonth(), d.year());
        zeroRates[i] = curve->zeroRate(current, QuantLib::ActualActual(), QuantLib::Continuous);
        d++;
    }
    
    //Rcpp::DataFrame frame = Rcpp::DataFrame::create(Rcpp::Named("date") = dates,
    //                                                Rcpp::Named("zeroRates") = zeroRates);
    Rcpp::List frame = Rcpp::List::create(Rcpp::Named("date") = dates,
                                          Rcpp::Named("zeroRates") = zeroRates);

    Rcpp::List rl = Rcpp::List::create(Rcpp::Named("times") = times,
                                       Rcpp::Named("discounts") = disc,
                                       Rcpp::Named("forwards") = fwds,
                                       Rcpp::Named("zerorates") = zero,
                                       Rcpp::Named("flatQuotes") = flatQuotes,
                                       Rcpp::Named("params") = rparams,
                                       Rcpp::Named("table") = frame);
    return rl;
}

