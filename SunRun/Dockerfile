FROM python:3-alpine
WORKDIR /app
COPY . /app
RUN pip install --trusted-host pypi.python.org -r requirements.txt
EXPOSE 8050

#CMD gunicorn -w 2 -b 0.0.0.0:8050 -t 100000 --max-requests 20 --reload user.app:server
CMD ["python", "user/app.py"]