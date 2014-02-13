rpt = {}

class entry:
	def __init__(self, last_address):
		self.last_address = la
		self.state = 0
	
	def update(self, new_address):
		new_delta = new_address - self.last_address
		if state:
			if new_delta == self.delta:
				prefetch(new_address, new_delta)
		else:
			self.state = 1
		self.delta = new_delta
		
def on_miss(program_counter, miss_address):
	if program_counter in rpt.keys:
		rpt[program_counter].update(miss_address)
	else:
		rpt[program_counter] = entry(miss_address)
