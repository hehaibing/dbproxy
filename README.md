# dbproxy
数据库访问中间层，封装redis及mysql的访问

# 数据库表结构
create table players(player_id varchar(128),prop_name varchar(64),prop_value blob, primary key(player_id,prop_name));

#编译

centos7

需要安装mysql connector c++, boost, protobuf

wget http://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-1.1.7-linux-el7-x86-64bit.rpm

rpm -Uvh mysql-connector-c++-1.1.7-linux-el7-x86-64bit.rpm

yum install epel-release

yum install -y protobuf-devel

yum install -y boost-devel

git clone https://github.com/hehaibing/dbproxy.git

`g++ -o dbproxy *.cpp proto/*.cc -Iinclude -Llibs/linux -luv -lhiredis -lpthread -lmysqlcppconn -lprotobuf`

add for test
add for test2
