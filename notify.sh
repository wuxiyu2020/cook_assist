#!/bin/bash

WECHAT_URL="https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=8e57a250-6c31-480e-8691-e760c1d7a82d"
TITLE=" "
CONTENT="## $CI_PROJECT_NAME构建失败：\n> 流水线地址: [$CI_PIPELINE_URL]($CI_PIPELINE_URL)"
curl -X POST -H "Content-Type: application/json;charset=utf-8" -d "{\"msgtype\": \"markdown\", \"markdown\": {\"title\":\"$TITLE\", \"content\": \"$CONTENT\"}}" $WECHAT_URL
