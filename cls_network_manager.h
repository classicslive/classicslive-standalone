#ifndef CLS_NETWORK_MANAGER_H
#define CLS_NETWORK_MANAGER_H

#include <QNetworkAccessManager>

#include <cl_frontend.h>

typedef struct
{
  void(*function)(cl_network_response_t);
} cls_net_cb;
Q_DECLARE_METATYPE(cls_net_cb);

class ClsNetworkManager : public QNetworkAccessManager
{
  Q_OBJECT

public:
  ClsNetworkManager();

signals:
  void request(const char *url, char *data, cls_net_cb callback);

public slots:
  void onFinished(QNetworkReply *reply);
  void onRequest(const char *url, char *data, cls_net_cb callback);

private:
  std::map<QNetworkReply*, cls_net_cb> m_Requests;
};

#endif
