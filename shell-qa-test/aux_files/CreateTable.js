session.sql('use sakila;').execute();
session.sql('drop table if exists testdb;').execute();
session.sql('create table testdb (name varchar(50), age integer, last_name varchar(100));').execute();