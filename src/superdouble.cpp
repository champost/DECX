
#include "superdouble.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
using namespace std;


Superdouble::Superdouble(long double m, int e):stilldouble(false),upperlimit(1e+100),lowerlimit(1e-100){
	mantissa=m;
	exponent=e;
	adjustDecimal();
}

Superdouble::~Superdouble() {}

int Superdouble::getExponent(){
	return exponent;
}

double Superdouble::getMantissa(){
	return mantissa;
}

void Superdouble::adjustDecimal() {
	stilldouble = false;
	if (mantissa==0 || isinf(mantissa) || isnan(mantissa)) {
		exponent=0;
		stilldouble = true;
	}
	else {
		while (fabsl(mantissa)>=10) {
			mantissa*=0.1;
			exponent+=1;
		}
		while (fabsl(mantissa)<1) {
			mantissa*=10.0;
			exponent+=-1;
		}
	}
}

ostream& operator<<(ostream& os, const Superdouble& x)
{
	os<<x.mantissa <<"e"<<x.exponent;
	return os;
}

Superdouble Superdouble::operator * ( Superdouble  x){
	return Superdouble(mantissa*x.mantissa,exponent+x.exponent);
}

Superdouble Superdouble::operator * ( double  x){
	return Superdouble(mantissa*x, exponent);
}


//add stilldouble
Superdouble Superdouble::operator / ( Superdouble  x){
	return Superdouble(mantissa/x.mantissa,exponent-x.exponent);
}

//add stilldouble
Superdouble Superdouble::operator + ( Superdouble  x){

	//only tricky thing is converting them to same exponent
	if (x.mantissa != 0) {	// (CBR 11.03.2014)
		if (mantissa!=0) {
			int exponentdif=x.exponent-exponent;
			return Superdouble(mantissa+(x.mantissa*(pow10l(exponentdif))),exponent);
		}
		else
			return Superdouble(x.mantissa,x.exponent);
	}
	else
		return Superdouble(mantissa, exponent);
}

//add stilldouble
Superdouble Superdouble::operator - ( Superdouble  x){
	//only tricky thing is converting them to same exponent
	if (x.mantissa != 0) {	// (CBR 11.03.2014)
		if (mantissa!=0) {
			int exponentdif=x.exponent-exponent;
			return Superdouble(mantissa-(x.mantissa*(pow10l(exponentdif))),exponent);
		}
		else
			return Superdouble(-1.0*x.mantissa,x.exponent);
	}
	else
		return Superdouble(mantissa, exponent);
}

//add stilldouble
void Superdouble::operator ++ (){
	mantissa++;
	adjustDecimal();
}

//add stilldouble
void Superdouble::operator -- (){
	mantissa--;
	adjustDecimal();
}

//add stilldouble
void Superdouble::operator *= (const Superdouble &x){
	mantissa*=x.mantissa;
	exponent+=x.exponent;
	adjustDecimal();
}

//add stilldouble
void Superdouble::operator /= (const Superdouble &x){
	mantissa/=x.mantissa;
	exponent-=x.exponent;
	adjustDecimal();
}

void Superdouble::operator += (const Superdouble  &x){
	//only tricky thing is converting them to same exponent
	if (x.mantissa != 0) {	// (CBR 11.03.2014)
		if (mantissa!=0) {
			if (stilldouble == true && x.stilldouble == true){
				mantissa += x.mantissa;
				if (fabsl(mantissa) > upperlimit || fabsl(mantissa) < lowerlimit){
					adjustDecimal();
				}
			}else{
				int exponentdif=x.exponent-exponent;
				mantissa=mantissa+(x.mantissa*(pow10l(exponentdif)));
				adjustDecimal();
			}
		}else {
			if (stilldouble == true && x.stilldouble == true){
				mantissa = x.mantissa;
				exponent=x.exponent;
			}else{
				mantissa=x.mantissa;
				exponent=x.exponent;
				adjustDecimal();
			}
		}
	}

}

//add stilldouble
void Superdouble::operator -= (const Superdouble  &x){
	//only tricky thing is converting them to same exponent
	if (x.mantissa != 0) {	// (CBR 11.03.2014)
		if (mantissa!=0) {
			int exponentdif=x.exponent-exponent;
			mantissa=mantissa-(x.mantissa*(pow10l(exponentdif)));
			adjustDecimal();
		}
		else {
			mantissa=-1.0*x.mantissa;
			exponent=x.exponent;
			adjustDecimal();
		}
	}
}

bool Superdouble::operator > (const Superdouble & x)const{
	if (exponent > x.exponent)
		return true;
	else if(exponent == x.exponent && mantissa > x.mantissa)
		return true;
	else
		return false;
}

bool Superdouble::operator >= (const Superdouble & x)const{
	if (exponent > x.exponent)
		return true;
	else if(exponent == x.exponent && mantissa >= x.mantissa)
		return true;
	else
		return false;
}

bool Superdouble::operator < (const Superdouble & x)const{
	if (exponent < x.exponent)
		return true;
	else if(exponent == x.exponent && mantissa < x.mantissa)
		return true;
	else
		return false;
}

bool Superdouble::operator <= (const Superdouble & x)const{
	if (exponent < x.exponent)
		return true;
	else if(exponent == x.exponent && mantissa <= x.mantissa)
		return true;
	else
		return false;
}

bool Superdouble::operator == (const Superdouble & x)const{
	if (exponent == x.exponent)
	    if (mantissa == x.mantissa)
		return true;
	    else
		return false;
	else
		return false;
}

bool Superdouble::operator != (const Superdouble & x)const{
	if (exponent != x.exponent)
	    return true;
	else
	    if (mantissa != x.mantissa)
		return true;
	    else
		return false;
}

//this just switches the sign of the superdouble
void Superdouble::switch_sign(){
	mantissa = -1.0*mantissa;
}

/*bool Superdouble::operator > (double x){
	if (double() > x)
		return true;
	else
		return false;
}*/

Superdouble Superdouble::getLn(){
	//ln(a * 10^b) = ln(a) + ln(10^b) = ln(a) + log10 (10^b) / log10 (e^1) = ln(a) + b/log10(e^1)
	//Superdouble result(logl(mantissa)+(1.0*(exponent))/log10l(exp(1)),0);

	//or (CBR 28.02.2014)
	//ln(a * 10^b) = ln(a) + ln(10^b) = ln(a) + b log(10)
	return Superdouble(logl(mantissa)+(double(exponent)*logl(10)),0);
}

Superdouble Superdouble::abs(){
	if (mantissa < 0)
		return Superdouble(-mantissa,exponent);
	else
		return Superdouble(mantissa,exponent);
}

