/******************************************************************************
 * Copyright © 2014-2017 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#define EXCHANGE_NAME "bitstamp"
#define UPDATE bitstamp ## _price
#define SUPPORTS bitstamp ## _supports
#define SIGNPOST bitstamp ## _signpost
#define TRADE bitstamp ## _trade
#define ORDERSTATUS bitstamp ## _orderstatus
#define CANCELORDER bitstamp ## _cancelorder
#define OPENORDERS bitstamp ## _openorders
#define TRADEHISTORY bitstamp ## _tradehistory
#define BALANCES bitstamp ## _balances
#define PARSEBALANCE bitstamp ## _parsebalance
#define WITHDRAW bitstamp ## _withdraw
#define EXCHANGE_AUTHURL "https://www.bitstamp.net/api"
#define CHECKBALANCE bitstamp ## _checkbalance
#define ALLPAIRS bitstamp ## _allpairs
#define FUNCS bitstamp ## _funcs
#define BASERELS bitstamp ## _baserels

static char *BASERELS[][2] = { {"btc","usd"} };
#include "exchange_supports.h"

double UPDATE(struct exchange_info *exchange,char *base,char *rel,struct exchange_quote *quotes,int32_t maxdepth,double commission,cJSON *argjson,int32_t invert)
{
    char url[1024];
    sprintf(url,"https://www.bitstamp.net/api/order_book/");
    return(exchanges777_standardprices(exchange,commission,base,rel,url,quotes,0,0,maxdepth,0,invert));
}

cJSON *SIGNPOST(void **cHandlep,int32_t dotrade,char **retstrp,struct exchange_info *exchange,char *method,char *payload)
{
    /*signature is a HMAC-SHA256 encoded message containing: nonce, customer ID (can be found here) and API key. The HMAC-SHA256 code must be generated using a secret key that was generated with your API key. This code must be converted to it's hexadecimal representation (64 uppercase characters).Example (Python):
     message = nonce + customer_id + api_key
     signature = hmac.new(API_SECRET, msg=message, digestmod=hashlib.sha256).hexdigest().upper()
     
     key - API key
     signature - signature
     nonce - nonce
     */
    char dest[1025],url[1024],req[1024],hdr1[512],hdr2[512],hdr3[512],hdr4[512],*sig,*data = 0;
    cJSON *json; uint64_t nonce;
    hdr1[0] = hdr2[0] = hdr3[0] = hdr4[0] = 0;
    nonce = exchange_nonce(exchange);
    sprintf(req,"%llu%s%s",(long long)nonce,exchange->userid,exchange->apikey);
    json = 0;
    if ( (sig= hmac_sha256_str(dest,exchange->apisecret,(int32_t)strlen(exchange->apisecret),req)) != 0 )
    {
        //touppercase(sig);
        //printf("req.(%s) sig.(%s)\n",req,sig);
        //sprintf(req,"{\"key\":\"%s\",\"signature\":\"%s\",\"nonce\":%llu%s}",exchange->apikey,sig,(long long)nonce,payload);
        sprintf(req,"key=%s&signature=%s&nonce=%llu%s",exchange->apikey,sig,(long long)nonce,payload);
        //printf("submit.(%s)\n",req);
        sprintf(url,"%s/%s/",EXCHANGE_AUTHURL,method);
        if ( dotrade == 0 )
            data = exchange_would_submit(req,hdr1,hdr2,hdr3,hdr4);
        else if ( (data= curl_post(&exchange->cHandle,url,0,req,req,hdr2,hdr3,hdr4)) != 0 )
            json = cJSON_Parse(data);
    }
    if ( retstrp != 0 )
        *retstrp = data;
    else if ( data != 0 )
        free(data);
    return(json);
}

char *PARSEBALANCE(struct exchange_info *exchange,double *balancep,char *coinstr,cJSON *argjson)
{
    char field[128],*itemstr = 0; cJSON *obj,*item;
    *balancep = 0.;
    strcpy(field,coinstr);
    tolowercase(field);
    if ( argjson != 0 && (obj= jobj(argjson,"return")) != 0 && (item= jobj(obj,"funds")) != 0 )
    {
        *balancep = jdouble(item,field);
        obj = cJSON_CreateObject();
        touppercase(field);
        jaddstr(obj,"base",field);
        jaddnum(obj,"balance",*balancep);
        itemstr = jprint(obj,1);
    }
    if ( itemstr == 0 )
        return(clonestr("{\"error\":\"cant find coin balance\"}"));
    return(itemstr);
}

cJSON *BALANCES(struct exchange_info *exchange,cJSON *argjson)
{
    return(SIGNPOST(&exchange->cHandle,1,0,exchange,"balance",""));
}

#include "checkbalance.c"

uint64_t TRADE(int32_t dotrade,char **retstrp,struct exchange_info *exchange,char *base,char *rel,int32_t dir,double price,double volume,cJSON *argjson)
{
    char payload[1024],url[512],pairstr[512],*extra; cJSON *json; uint64_t txid = 0;
    if ( (extra= *retstrp) != 0 )
        *retstrp = 0;
    if ( (dir= flipstr_for_exchange(exchange,pairstr,"%s%s",dir,&price,&volume,base,rel,argjson)) == 0 )
    {
        printf("cant find baserel (%s/%s)\n",base,rel);
        return(0);
    }
    sprintf(url,"%s/%s/",EXCHANGE_AUTHURL,dir>0 ? "buy" : "sell");
    if ( CHECKBALANCE(retstrp,dotrade,exchange,dir,base,rel,price,volume,argjson) == 0 && (json= SIGNPOST(&exchange->cHandle,dotrade,retstrp,exchange,url,payload)) != 0 )
    {
        // parse json and set txid
        free_json(json);
    }
    return(txid);
}

char *ORDERSTATUS(struct exchange_info *exchange,uint64_t quoteid,cJSON *argjson)
{
    char jsonbuf[128];
    sprintf(jsonbuf,"&id=%llu",(long long)quoteid);
    return(jprint(SIGNPOST(&exchange->cHandle,1,0,exchange,"order_status",jsonbuf),1));
}

char *CANCELORDER(struct exchange_info *exchange,uint64_t quoteid,cJSON *argjson)
{
    char jsonbuf[128];
    sprintf(jsonbuf,"&id=%llu",(long long)quoteid);
    return(jprint(SIGNPOST(&exchange->cHandle,1,0,exchange,"cancel_order",jsonbuf),1));
}

char *OPENORDERS(struct exchange_info *exchange,cJSON *argjson)
{
    return(jprint(SIGNPOST(&exchange->cHandle,1,0,exchange,"open_orders",""),1));
}

char *TRADEHISTORY(struct exchange_info *exchange,cJSON *argjson)
{
    return(jprint(SIGNPOST(&exchange->cHandle,1,0,exchange,"user_transactions",""),1));
}

char *WITHDRAW(struct exchange_info *exchange,char *base,double amount,char *destaddr,cJSON *argjson)
{
    return(clonestr("{\"error\":\"withdraw not yet\"}"));
}

struct exchange_funcs bitstamp_funcs = EXCHANGE_FUNCS(bitstamp,EXCHANGE_NAME);

#include "exchange_undefs.h"
