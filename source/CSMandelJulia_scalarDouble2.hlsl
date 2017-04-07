// Coded by Jan Vlietinck, 18 Oct 2009, V 1.8
//http://users.skynet.be/fquake/

#define UNROLL 32

RWTexture2D<float4> output : register (u[0]);

cbuffer cbCSMandel : register( b[0] )
{
	double a0, b0, da, db;
	double ja0, jb0; // julia set point
	
	int max_iterations;
	bool julia;  // julia or mandel
	int  cycle;
};

[numthreads(16, 16, 1)]
//****************************************************************************
void CSMandelJulia_scalarDouble2( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
//****************************************************************************
{ 
	int counter = max_iterations + 1;
	
	double u,v;
	u = (double)(a0 + da*DTid.x);
	v = (double)(b0 + db*DTid.y);

	double a,b;
	a = (julia) ? (double) ja0 : u;
	b = (julia) ? (double) jb0 : v;
  
	bool inside;
	double ur, vr, t;
	ur = u;
	vr = v;

	b = b*0.5;

	do
	{
		double uu,vv;
    
		uu = u*u;
		vv = v*v;
		inside = (uu+vv < 4.0) && (counter!=0);
		counter -= (inside) ? 1 : 0;		 
		v = u*v + b;     
		u = uu + a - vv;  
		v = v + v;  
    
		uu = u*u;
		vv = v*v;
		inside = (uu+vv < 4.0) && (counter!=0);
		counter -= (inside) ? 1 : 0;			  
		v = u*v + b; 	  
		u = uu + a - vv;  
		v = v + v;
  	  
	}  while ( inside); 	
	
	float4 color;
  
	float c = (float) (counter + cycle) * 2 * 3.1416f / 256.0f;  // color cycle
	color.r = (1 + cos(c)) / 2; 
	color.g = (1 + cos(2 * c + 2 * 3.1416f/3))/2; 
	color.b = (1 + cos(c - 2 * 3.1416f/3))/2; 
	color.a = 1;
  
	output[DTid.xy] = (counter == 0) ? 0 : color;
}




