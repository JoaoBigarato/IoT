from fastapi import FastAPI
from pydantic import BaseModel
import psycopg2
from psycopg2 import sql
from sqlalchemy import Column, Integer
from dotenv import load_dotenv
import os

# Criação da aplicação FastAPI
app = FastAPI()
load_dotenv()

HOST_DB = os.getenv('HOST_DB')
USER_DB = os.getenv('USER_DB')
PASSWORD_DB = os.getenv('PASSWORD_DB')
NAME_DB = os.getenv('NAME_DB')

# Definindo o modelo de dados esperado
class Monitoramento(BaseModel):
    tempoLigada:int = Column(Integer,nullable=False)
    tempoDesligada:int = Column(Integer,nullable=False)
    contagemPecas:int = Column(Integer,nullable=False)

# Função para conectar ao banco de dados PostgreSQL
def conectar_banco():
    try:
        # Conexão com o banco de dados PostgreSQL
        conn = psycopg2.connect(
            host=HOST_DB,  # Endereço do banco de dados
            user=USER_DB,    # Seu usuário do PostgreSQL
            password=PASSWORD_DB,  # Sua senha do PostgreSQL
            dbname=NAME_DB  # Nome do banco de dados
        )
        return conn
    except psycopg2.Error as err:
        print(f"Erro ao conectar ao banco de dados: {err}")
        return None

# Endpoint para receber os dados do ESP32
@app.post("/dados/")
async def receber_dados(monitoramento: Monitoramento):
    conn = conectar_banco()
    if conn:
        cursor = conn.cursor()
        query = """INSERT INTO "TesteArduino" (tempo_ligada, tempo_desligada, contagem_pecas)
                   VALUES (%s, %s, %s)"""
        valores = (monitoramento.tempoLigada, monitoramento.tempoDesligada, monitoramento.contagemPecas)
        
        try:
            cursor.execute(query, valores)
            conn.commit()  # Confirma a transação
            return {"status": "Dados enviados com sucesso!"}
        except psycopg2.Error as err:
            return {"status": f"Erro ao enviar dados: {err}"}
        finally:
            cursor.close()
            conn.close()

    return {"status": "Falha na conexão com o banco de dados"}
