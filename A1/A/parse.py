import json 
from tabulate import tabulate
import pandas as pd

f=open('dns.json')
data = json.load(f)

answers = []

for i in range(len(data)):
	try: 
		x=data[i]['_source']['layers']['dns']['Answers'].values()
		#print(data[i]['_source']['layers']['dns']['Answers'])
		for i in x:
			try:
				if(i['dns.a'] and i['dns.resp.name']):
					answer = {}
					print(i['dns.resp.name'])
					answer["resp_name"] = i['dns.resp.name']
					print(i['dns.a'])
					answer["a"] = i['dns.a']
					#print('\n');

					answers.append(answer)
			except:
				continue 
		
	except: 
		continue


df = pd.DataFrame(answers)
df.set_index('resp_name', inplace=True)

with open("ans.txt", "w") as f:
	# tabulate without indices
	f.write(tabulate(df, tablefmt='psql'))

f.close()