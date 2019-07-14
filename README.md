# 프로파일러 클래스
## 📢 개요
 프로파일러(profiler)는 코드의 실행 횟수와 소요 시간을 통계해서 성능 분석을 하는 프로그램이다. 
 
 단순한 프로그램을 성능 분석의 대상으로 삼는 것은 큰 의미가 없으나, MMO 서버와 같이 대규모의 데이터를 처리할 경우에는 약간의 시간 절약도 매우 큰 성능 향상으로 이어진다. 따라서, 프로파일링을 하면서 비효율적인 코드를 발견하고 효율적인 코드로 수정하는 것이 매우 중요하다. 

## 💻 PROFILER Example : LFMemoryPool vs. LFMemoryPool_TLS
  해당 프로그램은 락프리 메모리풀과 TLS를 활용한 락프리 메모리풀의 성능 차이를 비교할 수 있다.

  ![capture](https://github.com/kbm0996/MyProfiler/blob/master/figure/run.png)
  
  **figure 1. Program*
  
  ![capture](https://github.com/kbm0996/MyProfiler/blob/master/figure/result.png)
  
  **figure 2. PROFILER.txt*
 
## 🅿 사용법

 1. 성능 분석이 필요한 소스가 있는 헤더 파일에 해당 프로파일러를 include시킨다.
 
 2. 아래 매크로 함수를 이용하여 예제와 같이 사용한다.
 
 3. 프로그램을 종료하거나 PRINT()를 호출하면 프로그램이 있는 폴더에 PROFILER.txt 파일이 생성된다.


### ▶ 예제

    BEGIN(L"Code1");
    Code1();
    END(L"Code1");

