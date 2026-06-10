#include <cstdio>
#include <curl/curl.h>
#include <fstream>
#include <omp.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

FILE *gpLog;

struct TxData
{
  std::string sUuid;
  float fMnt;
};

static size_t
fnWrMem (void *pCont, size_t sz, size_t szNm, void *pUsr)
{
  size_t szRl;
  std::string *pMem;

  szRl = sz * szNm;
  pMem = static_cast<std::string *> (pUsr);
  pMem->append (static_cast<char *> (pCont), szRl);
  return szRl;
}

std::vector<TxData>
fnPCsv (const std::string &sFile)
{
  std::vector<TxData> vTx;
  std::ifstream ifsF;
  std::string sLine;
  int nLine;
  std::vector<std::string> vsCols;
  std::stringstream ssLn;
  std::string sItem;
  std::string sMnt;
  std::string sClid;
  float fMnt;

  ifsF.open (sFile);
  if (!ifsF.is_open ())
    {
      #pragma omp critical
      {
        fprintf (gpLog, "Error: Cannot open %s\n",
                 sFile.c_str ());
        fflush (gpLog);
      }
      return vTx;
    }

  if (std::getline (ifsF, sLine))
    {
      if (!sLine.empty () && sLine.back () == '\r')
        sLine.pop_back ();
      if (sLine.find ("MONTO APLICADO") == std::string::npos
          || sLine.find ("CODIGO CLIENTE") == std::string::npos)
        {
          #pragma omp critical
          {
            fprintf (gpLog, "Error: Bad header in %s\n",
                     sFile.c_str ());
            fflush (gpLog);
          }
          return vTx;
        }
    }
  else
    {
      #pragma omp critical
      {
        fprintf (gpLog, "Error: Empty file %s\n",
                 sFile.c_str ());
        fflush (gpLog);
      }
      return vTx;
    }

  nLine = 1;
  while (std::getline (ifsF, sLine))
    {
      nLine++;
      if (!sLine.empty () && sLine.back () == '\r')
        sLine.pop_back ();

      vsCols.clear ();
      ssLn.clear ();
      ssLn.str (sLine);

      while (std::getline (ssLn, sItem, ';'))
        {
          if (!sItem.empty () && sItem.front () == '"')
            sItem.erase (0, 1);
          if (!sItem.empty () && sItem.back () == '"')
            sItem.pop_back ();
          vsCols.push_back (sItem);
        }

      if (!sLine.empty () && sLine.back () == ';')
        vsCols.push_back ("");

      if (vsCols.size () != 10)
        {
          #pragma omp critical
          {
            fprintf (gpLog,
                     "Error: %s line %d cols %zu\n",
                     sFile.c_str (), nLine,
                     vsCols.size ());
            fflush (gpLog);
          }
          return std::vector<TxData> ();
        }

      sMnt = vsCols[6];
      sClid = vsCols[9];

      try
        {
          fMnt = std::stof (sMnt);
          vTx.push_back ({sClid, fMnt});
        }
      catch (const std::exception &exErr)
        {
          #pragma omp critical
          {
            fprintf (gpLog,
                     "Error: Bad MONTO %s line %d: %s\n",
                     sFile.c_str (), nLine,
                     exErr.what ());
            fflush (gpLog);
          }
          return std::vector<TxData> ();
        }
    }

  return vTx;
}

void
fnDlPr (std::vector<TxData> &vTxAll)
{
  std::string sUrl;
  std::string sPwd;

  sUrl = "sftp://137.184.45.251/";
  sPwd = "utem:CPyD.2026";

  #pragma omp parallel
  {
    #pragma omp single
    {
      CURL *pCurl;
      CURLcode cRes;
      std::string sDir;

      pCurl = curl_easy_init ();
      if (pCurl)
        {
          curl_easy_setopt (pCurl, CURLOPT_URL,
                            sUrl.c_str ());
          curl_easy_setopt (pCurl, CURLOPT_USERPWD,
                            sPwd.c_str ());
          curl_easy_setopt (pCurl,
                            CURLOPT_SSL_VERIFYPEER, 0L);
          curl_easy_setopt (pCurl,
                            CURLOPT_SSL_VERIFYHOST, 0L);
          curl_easy_setopt (pCurl,
                            CURLOPT_DIRLISTONLY, 1L);
          curl_easy_setopt (pCurl,
                            CURLOPT_WRITEFUNCTION,
                            fnWrMem);
          curl_easy_setopt (pCurl,
                            CURLOPT_WRITEDATA, &sDir);

          printf ("Fetching file list...\n");
          cRes = curl_easy_perform (pCurl);

          if (cRes != CURLE_OK)
            {
              fprintf (gpLog, "Error listing: %s\n",
                       curl_easy_strerror (cRes));
              fflush (gpLog);
            }
          else
            {
              std::regex rxCsv;
              std::smatch smMat;
              std::string::const_iterator itSrc;
              std::vector<std::string> vsFile;

              rxCsv = std::regex (
                "(reporte_\\d+\\.csv)");
              itSrc = sDir.cbegin ();

              while (std::regex_search (
                itSrc, sDir.cend (), smMat, rxCsv))
                {
                  vsFile.push_back (smMat[1]);
                  itSrc = smMat.suffix ().first;
                }

              printf ("Found %zu CSV files\n",
                      vsFile.size ());

              for (const auto &sFile : vsFile)
                {
                  if (access (sFile.c_str (),
                              F_OK) == 0)
                    {
                      #pragma omp task \
                        firstprivate(sFile) \
                        shared(vTxAll)
                      {
                        std::vector<TxData> vLTx;

                        vLTx = fnPCsv (sFile);
                        #pragma omp critical
                        {
                          vTxAll.insert (
                            vTxAll.end (),
                            vLTx.begin (),
                            vLTx.end ());
                        }
                      }
                    }
                  else
                    {
                      #pragma omp task \
                        firstprivate(sFile, sUrl, sPwd) \
                        shared(vTxAll)
                      {
                        std::string sFurl;
                        bool bOk;
                        int nMaxR;
                        int nAtt;
                        FILE *pOutF;
                        CURL *pTCrl;
                        CURLcode cTRes;
                        std::vector<TxData> vLTx;

                        sFurl = sUrl + sFile;
                        bOk = false;
                        nMaxR = 3;

                        for (nAtt = 1;
                             nAtt <= nMaxR; ++nAtt)
                          {
                            pOutF = fopen (
                              sFile.c_str (), "wb");
                            if (!pOutF)
                              {
                                #pragma omp critical
                                {
                                  fprintf (gpLog,
                                    "Error: create %s\n",
                                    sFile.c_str ());
                                  fflush (gpLog);
                                }
                                break;
                              }

                            pTCrl = curl_easy_init ();
                            if (pTCrl)
                              {
                                curl_easy_setopt (pTCrl,
                                  CURLOPT_URL,
                                  sFurl.c_str ());
                                curl_easy_setopt (pTCrl,
                                  CURLOPT_USERPWD,
                                  sPwd.c_str ());
                                curl_easy_setopt (pTCrl,
                                  CURLOPT_SSL_VERIFYPEER,
                                  0L);
                                curl_easy_setopt (pTCrl,
                                  CURLOPT_SSL_VERIFYHOST,
                                  0L);
                                curl_easy_setopt (pTCrl,
                                  CURLOPT_DIRLISTONLY,
                                  0L);
                                curl_easy_setopt (pTCrl,
                                  CURLOPT_TIMEOUT, 30L);
                                curl_easy_setopt (pTCrl,
                                  CURLOPT_WRITEDATA,
                                  pOutF);

                                cTRes =
                                  curl_easy_perform (
                                    pTCrl);
                                if (cTRes == CURLE_OK)
                                  bOk = true;
                                else
                                  {
                                    #pragma omp critical
                                    {
                                      fprintf (gpLog,
                                        "Error dl %s:"
                                        " %s\n",
                                        sFile.c_str (),
                                        curl_easy_strerror (
                                          cTRes));
                                      fflush (gpLog);
                                    }
                                  }
                                curl_easy_cleanup (
                                  pTCrl);
                              }
                            fclose (pOutF);
                            if (bOk)
                              break;
                            bOk = false;
                          }

                        if (bOk)
                          {
                            vLTx = fnPCsv (sFile);
                            #pragma omp critical
                            {
                              vTxAll.insert (
                                vTxAll.end (),
                                vLTx.begin (),
                                vLTx.end ());
                            }
                          }
                        else
                          {
                            #pragma omp critical
                            {
                              fprintf (gpLog,
                                "Failed %s after"
                                " %d tries\n",
                                sFile.c_str (),
                                nMaxR);
                              fflush (gpLog);
                            }
                            unlink (sFile.c_str ());
                          }
                      }
                    }
                }
            }
          curl_easy_cleanup (pCurl);
        }
      #pragma omp taskwait
    }
  }
}

void
fnGndr (const std::set<std::string> &setUid,
        std::unordered_map<std::string, std::string>
          &mapGnd)
{
  std::vector<std::string> vsUid;
  std::vector<std::string> vsRes;
  int nDone;
  int nTot;
  size_t szI;

  vsUid.assign (setUid.begin (), setUid.end ());
  vsRes.resize (vsUid.size (), "UNKNOWN");
  nDone = 0;
  nTot = static_cast<int> (vsUid.size ());

  printf ("API lookups: %d UUIDs\n", nTot);

  #pragma omp parallel for schedule(dynamic)
  for (szI = 0; szI < vsUid.size (); ++szI)
    {
      std::string sUrl;
      CURL *pCurl;
      std::string sResp;
      CURLcode cRes;
      long lCode;
      std::string sKey;
      size_t szPos;
      size_t szEnd;
      int nCur;

      sUrl = "https://api.sebastian.cl/cpyd/v1/person/"
             + vsUid[szI];
      pCurl = curl_easy_init ();
      if (pCurl)
        {
          curl_easy_setopt (pCurl, CURLOPT_URL,
                            sUrl.c_str ());
          curl_easy_setopt (pCurl,
                            CURLOPT_TIMEOUT, 10L);
          curl_easy_setopt (pCurl,
                            CURLOPT_WRITEFUNCTION,
                            fnWrMem);
          curl_easy_setopt (pCurl,
                            CURLOPT_WRITEDATA, &sResp);
          curl_easy_setopt (pCurl,
                            CURLOPT_SSL_VERIFYPEER, 0L);
          curl_easy_setopt (pCurl,
                            CURLOPT_SSL_VERIFYHOST, 0L);

          cRes = curl_easy_perform (pCurl);
          lCode = 0;
          curl_easy_getinfo (pCurl,
                             CURLINFO_RESPONSE_CODE,
                             &lCode);

          if (cRes == CURLE_OK && lCode == 200)
            {
              sKey = "\"gender\": \"";
              szPos = sResp.find (sKey);
              if (szPos != std::string::npos)
                {
                  szPos += sKey.length ();
                  szEnd = sResp.find ("\"", szPos);
                  if (szEnd != std::string::npos)
                    vsRes[szI] = sResp.substr (
                      szPos, szEnd - szPos);
                }
            }

          #pragma omp atomic capture
          {
            nDone++;
            nCur = nDone;
          }

          if (nCur % 100 == 0 || nCur == nTot)
            {
              #pragma omp critical
              {
                printf ("API: %d / %d\n", nCur, nTot);
              }
            }

          curl_easy_cleanup (pCurl);
        }
    }

  for (szI = 0; szI < vsUid.size (); ++szI)
    mapGnd[vsUid[szI]] = vsRes[szI];
}

void
fnCalc (const std::vector<TxData> &vTx,
        const std::unordered_map<std::string,
          std::string> &mapGnd,
        double dTime)
{
  using MapCIt
    = std::unordered_map<std::string,
        std::string>::const_iterator;

  double dMntF;
  double dMntM;
  long lTxF;
  long lTxM;
  double dAvgF;
  double dAvgM;
  FILE *pFRes;
  size_t szI;

  dMntF = 0.0;
  dMntM = 0.0;
  lTxF = 0;
  lTxM = 0;

  #pragma omp parallel for \
    reduction(+:dMntF, dMntM, lTxF, lTxM)
  for (szI = 0; szI < vTx.size (); ++szI)
    {
      MapCIt itGnd;

      itGnd = mapGnd.find (vTx[szI].sUuid);
      if (itGnd != mapGnd.end ())
        {
          if (itGnd->second == "FEMENINO")
            {
              dMntF += vTx[szI].fMnt;
              lTxF++;
            }
          else if (itGnd->second == "MASCULINO")
            {
              dMntM += vTx[szI].fMnt;
              lTxM++;
            }
        }
    }

  dAvgF = (lTxF > 0) ? (dMntF / lTxF) : 0.0;
  dAvgM = (lTxM > 0) ? (dMntM / lTxM) : 0.0;

  printf ("FEMENINO = %f\n", dAvgF);
  printf ("MASCULINO = %f\n", dAvgM);
  printf ("TIEMPO = %f segundos\n", dTime);

  pFRes = fopen ("resultados.txt", "w");
  if (pFRes)
    {
      fprintf (pFRes, "FEMENINO = %f\n", dAvgF);
      fprintf (pFRes, "MASCULINO = %f\n", dAvgM);
      fprintf (pFRes, "TIEMPO = %f segundos\n", dTime);
      fclose (pFRes);
    }
  else
    {
      fprintf (gpLog,
               "Error: cannot write resultados.txt\n");
      fflush (gpLog);
    }
}

int
main (int nArgc, char **vArg)
{
  double dStart;
  double dEnd;
  double dTime;
  std::vector<TxData> vTxAll;
  std::set<std::string> setUid;
  std::unordered_map<std::string, std::string> mapGnd;

  gpLog = fopen ("log.txt", "w");

  if (nArgc > 1 && std::string (vArg[1]) == "--seq")
    omp_set_num_threads (1);

  dStart = omp_get_wtime ();

  curl_global_init (CURL_GLOBAL_DEFAULT);

  fnDlPr (vTxAll);
  printf ("Transactions: %zu\n", vTxAll.size ());

  for (const auto &td : vTxAll)
    setUid.insert (td.sUuid);
  printf ("Unique UUIDs: %zu\n", setUid.size ());

  fnGndr (setUid, mapGnd);

  curl_global_cleanup ();

  dEnd = omp_get_wtime ();
  dTime = dEnd - dStart;

  fnCalc (vTxAll, mapGnd, dTime);

  fclose (gpLog);

  return 0;
}