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
void CSMandelJulia_vectorDouble( uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex )
//****************************************************************************
{ 
	int4 counter = max_iterations +1 ;
	
	double4 u,v;
	u = (a0 + da*double4(2*DTid.x, 2*DTid.x+1, 2*DTid.x, 2*DTid.x+1));
	v = (b0 + db*double4(2*DTid.y, 2*DTid.y, 2*DTid.y+1, 2*DTid.y+1));

	double4 a,b;
	a = (julia) ? (double4) ja0 : u;
	b = (julia) ? (double4) jb0 : v;

	bool4 inside;
   
	b = b*0.5;

	do
	{
		double4 uu,vv;
    
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
	} while ( any(inside)); 	
  
	float4x4 color;
  
	float4 c = (float4) (counter + cycle) * 2 * 3.1416f / 256.0f;  // color cycle
	color[0] = (1 + cos(c)) / 2; 
	color[1] = (1 + cos(2 * c + 2 * 3.1416f / 3)) / 2; 
	color[2] = (1 + cos(  c - 2*3.1416f/3)) / 2; 
	color[3] = 1;
  
	color = transpose(color);
  
	output[2*DTid.xy]            = (counter.x == 0) ? 0 : color[0];
	output[2*DTid.xy+uint2(1,0)] = (counter.y == 0) ? 0 : color[1];
	output[2*DTid.xy+uint2(0,1)] = (counter.z == 0) ? 0 : color[2];
	output[2*DTid.xy+uint2(1,1)] = (counter.w == 0) ? 0 : color[3];
}

