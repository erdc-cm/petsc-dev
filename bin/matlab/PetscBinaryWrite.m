function PetscBinaryWrite(filename,varargin)
%
%  Writes in PETSc binary file sparse matrices and vectors
%  if the array is multidimensional and dense it is saved
%  as a one dimensional array
%
fd = fopen(filename,'w','ieee-be');
for l=1:nargin-1
  A = varargin{l};
  if issparse(A)
    % save sparse matrix in special Matlab format
    [m,n] = size(A);
    majic = 1.2345678910e-30;
    for i=1:min(m,n)
      if A(i,i) == 0
        A(i,i) = majic;
      end
    end
    if min(size(A)) == 1     %a one-rank matrix will be compressed to a
                             %scalar instead of a vectory by sum
      n_nz = full(A' ~= 0);
    else
      n_nz = full(sum(A' ~= 0));
    end
    nz   = sum(n_nz);
    header = fwrite(fd,[1211216,m,n,nz],'int32');

    fwrite(fd,n_nz,'int32');  %nonzeros per row
    [i,j,s] = find((A' ~= 0).*(A'));
    fwrite(fd,i-1,'int32');
    for i=1:nz
      if s(i) == majic
        s(i) = 0;
      end
    end
    fwrite(fd,s,'double');
  else
    [m,n] = size(A);
    fwrite(fd,[1211214,m*n],'int32');
    fwrite(fd,A,'double');
  end
end
fclose(fd);
