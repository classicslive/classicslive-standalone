#include <QJsonArray>
#include <QNetworkReply>

#include <cl_frontend.h>

extern "C"
{
  #include <cl_common.h>
  #include <cl_json.h>
}

#include "cls_network_manager.h"

ClsNetworkManager::ClsNetworkManager()
{
  qRegisterMetaType<cls_net_cb>();
  connect(this, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(onFinished(QNetworkReply*)));
  connect(this, SIGNAL(request(const char*, char*, cls_net_cb)),
          this, SLOT(onRequest(const char*, char*, cls_net_cb)));
}

void ClsNetworkManager::onFinished(QNetworkReply *reply)
{
  auto cb = m_Requests.at(reply);
  cl_network_response_t response;
  auto error_msg = reply->errorString().toStdString();

  response.error_code = reply->error();
  response.error_msg = error_msg.c_str();

  if (response.error_code)
    cl_fe_display_message(CL_MSG_ERROR, response.error_msg);

  QByteArray response_array = reply->readAll();
  response.data = response_array.data();

  bool success;
  if (cl_json_get(&success, response.data, "success", CL_JSON_BOOLEAN, 1) && !success)
  {
    char reason[2048];

    if (cl_json_get(reason, response.data, "reason", CL_JSON_STRING, sizeof(reason)))
      cl_fe_display_message(CL_MSG_ERROR, reason);
    else
      cl_fe_display_message(CL_MSG_ERROR, "Unknown network error.");
  }

  if (cb.function)
    cb.function(response);

  m_Requests.erase(reply);
}

void ClsNetworkManager::onRequest(const char *url, char *data, cls_net_cb callback)
{
  QUrl url_data = QUrl(url);
  if (!url_data.isValid())
    return;

  QNetworkRequest request;
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/x-www-form-urlencoded"));
#ifdef GIT_VERSION
  request.setRawHeader("User-Agent", QStringLiteral("classicslive-standalone %1").arg(GIT_VERSION).toUtf8());
#else
  request.setRawHeader("User-Agent", "classicslive-standalone");
#endif
  request.setUrl(QUrl(url));

  QByteArray post_data;
  post_data.append(data);

  free(data);

  QNetworkReply *reply = post(request, post_data);
  m_Requests.insert(std::pair<QNetworkReply*, cls_net_cb>(reply, callback));
}
